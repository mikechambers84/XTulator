cmake_minimum_required(VERSION 3.16)
project(xtulator)

set(CMAKE_C_FLAGS "-O3")

file(GLOB XT XTulator/*.c)
file(GLOB chipset XTulator/chipset/*.c)
file(GLOB cpu XTulator/cpu/*.c)
file(GLOB audio XTulator/modules/audio/*.c)
file(GLOB disk XTulator/modules/disk/*.c)
file(GLOB input XTulator/modules/input/*.c)
file(GLOB io XTulator/modules/io/*.c)
file(GLOB video XTulator/modules/video/*.c)
add_executable(xtulator ${XT} ${chipset} ${cpu} ${audio} ${disk} ${input} ${io} ${video})

target_link_libraries(xtulator m pthread SDL2 pcap)
