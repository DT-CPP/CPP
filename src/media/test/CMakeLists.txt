include(CTest)

SET(LINK_DEPS
	xstudio::media
	xstudio::audio_output
	xstudio::global
	caf::core
)

create_tests("${LINK_DEPS}")
