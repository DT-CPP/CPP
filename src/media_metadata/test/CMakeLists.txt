include(CTest)

SET(LINK_DEPS
	xstudio::media_metadata
	xstudio::global
	caf::core
)

create_tests("${LINK_DEPS}")
