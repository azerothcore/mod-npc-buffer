AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/npc_buffer.cpp")
AC_ADD_SCRIPT_LOADER("NPCBuffer" "${CMAKE_CURRENT_LIST_DIR}/src/loader_buffer.h")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/npc_buffer.conf.dist")