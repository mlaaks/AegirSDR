project (AegirSDR)

set(SOURCES main.cc csdrdevice.cc crtlsdr.cc chackrf.cc console.cc ccoherent.cc cdsp.cc ccontrol.cc ctransport.cc ctransportsoapy.cc common.cc yamlparser.cc)

set(GPUFFT_DIR /opt/vc/src/hello_pi/hello_fft)

set(GPUFFT_SOURCES     ${GPUFFT_DIR}/mailbox.c
                       ${GPUFFT_DIR}/gpu_fft.c 
                       ${GPUFFT_DIR}/gpu_fft_base.c
                       ${GPUFFT_DIR}/gpu_fft_twiddles.c
                       ${GPUFFT_DIR}/gpu_fft_shaders.c)


if (RASPBERRYPI)
	include_directories(${GPUFFT_DIR})
    include_directories(${GPUFFT_DIR}/hex)
	
	add_executable(AegirSDR ${SOURCES} ${GPUFFT_SOURCES})
	target_link_libraries(AegirSDR zmq pthread fftw3f volk rtlsdr stdc++ dl rt)
else (RASPBERRYPI)
	
	add_executable(AegirSDR ${SOURCES})
	target_link_libraries(AegirSDR zmq pthread fftw3f volk rtlsdr hackrf stdc++ yaml-cpp)

	add_custom_command(
        TARGET AegirSDR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/yml/config.yml
                ${CMAKE_CURRENT_BINARY_DIR}/config.yml)
        add_custom_command(
        TARGET AegirSDR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/yml/confighackrf.yml
                ${CMAKE_CURRENT_BINARY_DIR}/confighackrf.yml)

endif (RASPBERRYPI)

