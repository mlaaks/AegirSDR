project (AegirSDR)

set(SOURCES main.cc csdrdevice.cc crtlsdr.cc console.cc ccoherent.cc cdsp.cc ccontrol.cc ctransport.cc common.cc)

set(GPUFFT_DIR /opt/vc/src/hello_pi/hello_fft)

set(GPUFFT_SOURCES     ${GPUFFT_DIR}/mailbox.c
                       ${GPUFFT_DIR}/gpu_fft.c 
                       ${GPUFFT_DIR}/gpu_fft_base.c
                       ${GPUFFT_DIR}/gpu_fft_twiddles.c
                       ${GPUFFT_DIR}/gpu_fft_shaders.c)

find_package(Python COMPONENTS Interpreter Development)

message("Python_FOUND:${Python_FOUND}")
message("Python_VERSION:${Python_VERSION}")
message("Python_Development_FOUND:${Python_Development_FOUND}")
message("Python_LIBRARIES:${Python_LIBRARIES}")

if (RASPBERRYPI)
	include_directories(${GPUFFT_DIR})
    include_directories(${GPUFFT_DIR}/hex)
	
	add_executable(AegirSDR ${SOURCES} ${GPUFFT_SOURCES})
	target_link_libraries(AegirSDR zmq pthread fftw3f volk rtlsdr stdc++ dl rt)
else (RASPBERRYPI)
	include_directories(${Python_INCLUDE_DIRS})

	add_executable(AegirSDR ${SOURCES})
	target_link_libraries(AegirSDR zmq pthread fftw3f volk rtlsdr stdc++ ${Python_LIBRARIES})
endif (RASPBERRYPI)

