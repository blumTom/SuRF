add_executable(workload bench/workload.cpp)
target_link_libraries(workload surf)

add_executable(workload_multi_thread bench/workload_multi_thread.cpp)
target_link_libraries(workload_multi_thread surf)

#add_executable(workload_arf bench/workload_arf.cpp)
#target_link_libraries(workload_arf ARF)