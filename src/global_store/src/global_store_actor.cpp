// SPDX-License-Identifier: Apache-2.0

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace caf;

namespace xstudio::global_store {


GlobalStoreActor::GlobalStoreActor(
    caf::actor_config &cfg, const JsonStore &jsn, std::string reg_value)
    : caf::event_based_actor(cfg),
      reg_value_(std::move(reg_value)),
      base_(static_cast<JsonStore>(jsn["base"])) {
    init();
}

GlobalStoreActor::GlobalStoreActor(
    caf::actor_config &cfg,
    const std::string &name,
    const JsonStore &jsn,
    std::string reg_value)
    : caf::event_based_actor(cfg), reg_value_(std::move(reg_value)), base_(name) {
    base_.preferences_ = jsn;
    init();
}

GlobalStoreActor::GlobalStoreActor(
    caf::actor_config &cfg,
    const std::string &name,
    const std::vector<GlobalStoreDef> &defs,
    std::string reg_value)
    : caf::event_based_actor(cfg), reg_value_(std::move(reg_value)), base_(name) {
    for (const auto &i : defs) {
        base_.preferences_[nlohmann::json::json_pointer(i.path_)] = i;
    }

    init();
}

void GlobalStoreActor::init() {
    // only parial..
    spdlog::debug("Created GlobalStoreActor {}", base_.name());
    print_on_exit(this, "GlobalStoreActor");

    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);
    auto jsonactor =
        spawn<JsonStoreActor>(Uuid(), base_.preferences_, std::chrono::milliseconds(50));
    link_to(jsonactor);

    // link to store, so we can get our own settings.
    try {
        caf::scoped_actor sys(system());
        auto result = request_receive<std::pair<caf::actor, JsonStore>>(
            *sys, jsonactor, utility::get_group_atom_v);

        request_receive<bool>(*sys, result.first, broadcast::join_broadcast_atom_v, this);

        base_.preferences_.set(result.second);
        base_.autosave_interval_ =
            preference_value<int>(base_.preferences_, "/core/global_store/autosave_interval");
    } catch (...) {
    }

    system().registry().put(reg_value_, this);

    behavior_.assign(
        // base_.make_set_name_handler(event_group_, this),
        base_.make_get_name_handler(),
        base_.make_last_changed_getter(),
        base_.make_last_changed_setter(event_group_, this),
        base_.make_last_changed_event_handler(event_group_, this),
        base_.make_get_uuid_handler(),
        base_.make_get_type_handler(),
        make_get_event_group_handler(event_group_),
        base_.make_get_detail_handler(this, event_group_),


        [=](autosave_atom) -> bool { return base_.autosave_; },

        [=](autosave_atom, const bool enable) {
            if (enable != base_.autosave_)
                send(event_group_, utility::event_atom_v, autosave_atom_v, enable);
            base_.autosave_ = enable;
            if (base_.autosave_)
                delayed_anon_send(
                    actor_cast<caf::actor>(this),
                    std::chrono::seconds(base_.autosave_interval_),
                    do_autosave_atom_v);
        },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](const group_down_msg & /*msg*/) {
            // 		if(msg.source == store_events)
            // unsubscribe();
        },

        [=](do_autosave_atom) {
            if (base_.autosave_)
                delayed_anon_send(
                    actor_cast<caf::actor>(this),
                    std::chrono::seconds(base_.autosave_interval_),
                    do_autosave_atom_v);

            for (const auto &context : PreferenceContexts)
                request(actor_cast<caf::actor>(this), infinite, save_atom_v, context)
                    .then(
                        [=](const bool result) mutable {
                            if (result)
                                spdlog::debug("Autosaved {}", context);
                            else
                                spdlog::warn("Autosave failed {}", context);
                        },
                        [=](const error &err) {
                            spdlog::warn("Autosave failed {} {}.", context, to_string(err));
                        });
        },

        [=](get_json_atom atom) { delegate(jsonactor, atom); },

        [=](get_json_atom atom, const std::string &path) { delegate(jsonactor, atom, path); },

        [=](json_store::update_atom atom,
            const JsonStore &change,
            const std::string &path,
            const JsonStore &full) {
            send(event_group_, utility::event_atom_v, atom, change, path);
            delegate(actor_cast<caf::actor>(this), atom, full);
        },

        [=](json_store::update_atom, const JsonStore &json) {
            base_.preferences_.set(json);
            try {
                auto tmp = preference_value<int>(
                    base_.preferences_, "/core/global_store/autosave_interval");
                if (tmp != base_.autosave_interval_) {
                    base_.autosave_interval_ = tmp;
                    spdlog::debug("autosave updated {}", base_.autosave_interval_);
                }
            } catch (...) {
            }
        },

        [=](save_atom, const std::string &context) -> caf::result<bool> {
            // collect items for context
            // make sure we're uptodate with jsonstore..
            caf::scoped_actor sys(system());
            const std::string path = preference_path_context(context);

            if (not path.empty()) {
                if (not check_preference_path())
                    return caf::result<bool>(make_error(
                        xstudio_error::error, fmt::format("Failed to save {}", context)));

                // update our copy
                base_.preferences_.set(
                    request_receive<JsonStore>(*sys, jsonactor, json_store::get_json_atom_v));

                // get things to store..
                JsonStore prefs = get_preference_values(
                    base_.preferences_, std::set<std::string>{context}, true, path);

                // check dir exists..
                std::ofstream o(path + ".tmp", std::ofstream::out | std::ofstream::trunc);
                try {
                    o.exceptions(std::ios_base::failbit | std::ifstream::badbit);

                    o << std::setw(4) << prefs.cref() << std::endl;
                    o.close();

                    fs::rename(path + ".tmp", path);

                    spdlog::debug("Saved {} {}", context, path);
                } catch (const std::exception &err) {
                    if (o.is_open()) {
                        o.close();
                        fs::remove(path + ".tmp");
                    }
                    return caf::result<bool>(make_error(
                        xstudio_error::error,
                        fmt::format("Failed to save {} {}", context, err.what())));
                }
            } else {
                return caf::result<bool>(make_error(
                    xstudio_error::error, fmt::format("Invalid context {}", context)));
            }

            return caf::result<bool>(true);
        },

        [=](utility::serialise_atom, const std::string &context) -> JsonStore {
            caf::scoped_actor sys(system());
            base_.preferences_.set(
                request_receive<JsonStore>(*sys, jsonactor, json_store::get_json_atom_v));
            JsonStore result;

            try {
                if (context.empty()) {
                    result = base_.serialise();
                } else {
                    // extract context..
                    const std::string path = preference_path_context(context);
                    result                 = get_preference_values(
                        base_.preferences_, std::set<std::string>{context}, true, path);
                }
            } catch (...) {
            }
            return result;
        },

        [=](set_json_atom atom, const JsonStore &json) { delegate(jsonactor, atom, json); },

        [=](set_json_atom atom, const JsonStore &json, const std::string &path) {
            delegate(jsonactor, atom, json, path);
        },

        [=](set_json_atom atom,
            const JsonStore &json,
            const std::string &path,
            const bool broadcast) { delegate(jsonactor, atom, json, path, false, broadcast); },

        [=](utility::get_group_atom atom) { delegate(jsonactor, atom); }


    );
}

void GlobalStoreActor::on_exit() { system().registry().erase(reg_value_); }
} // namespace xstudio::global_store
