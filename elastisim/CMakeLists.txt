# Copyright (c) 2022, Technical University of Darmstadt, Germany
# Modifications Copyright (c) 2026 Fulda University of Applied Sciences, Germany
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(elastisim)

option(ELASTISIM_NATIVE_MARCH "Compile with -march=native for local CPU optimized runs" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep this source tree independent of any surrounding reproduction workflow.
set(SIMGRID_ROOT "" CACHE PATH "SimGrid installation prefix (required)")
if(NOT SIMGRID_ROOT)
  message(FATAL_ERROR "Set SIMGRID_ROOT to the installation prefix of SimGrid 4.1.")
endif()
if(NOT EXISTS "${SIMGRID_ROOT}/include/simgrid/s4u.hpp")
  message(FATAL_ERROR "SIMGRID_ROOT does not contain include/simgrid/s4u.hpp: ${SIMGRID_ROOT}")
endif()
set(SIMGRID_SOURCE_DIR "${SIMGRID_ROOT}")
set(BOOST_ROOT "" CACHE PATH "Optional Boost include root for locally downloaded Boost headers")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options("$<$<CONFIG:Release>:-O3>")
  add_compile_options("$<$<CONFIG:Release>:-DNDEBUG>")
  if(ELASTISIM_NATIVE_MARCH)
    add_compile_options("-march=native")
  endif()
endif()

set(ELASTISIM_INCLUDE_DIRS
    ${SIMGRID_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/interface
    ${PROJECT_SOURCE_DIR}/src/software
    ${PROJECT_SOURCE_DIR}/src/system
    ${PROJECT_SOURCE_DIR}/src/system/messages
    ${PROJECT_SOURCE_DIR}/src/tasks
    ${PROJECT_SOURCE_DIR}/src/util
    ${PROJECT_SOURCE_DIR}/src/scheduling
    ${PROJECT_SOURCE_DIR}/src/scheduling/utils
    ${PROJECT_SOURCE_DIR}/src/scheduling/base
    ${PROJECT_SOURCE_DIR}/src/scheduling/schedulers
    ${PROJECT_SOURCE_DIR}/src/scheduling/malleability
    ${PROJECT_SOURCE_DIR}/third-party/exprtk
    ${PROJECT_SOURCE_DIR}/third-party/nlohmann_json
    ${PROJECT_SOURCE_DIR}/third-party/indicators
)

if(BOOST_ROOT)
  list(APPEND ELASTISIM_INCLUDE_DIRS ${BOOST_ROOT})
endif()

include_directories(${ELASTISIM_INCLUDE_DIRS})

add_executable(elastisim
    main.cpp
    src/system/SimulationEngine.cpp src/system/SimulationEngine.h
    src/software/Job.cpp src/software/Job.h
    src/system/Scheduler.cpp src/system/Scheduler.h
    src/system/Node.cpp src/system/Node.h
    src/util/Utility.cpp src/util/Utility.h
    src/ElastiSim.cpp src/ElastiSim.h
    src/system/PeriodicInvoker.cpp src/system/PeriodicInvoker.h
    src/software/Workload.cpp src/software/Workload.h
    src/system/PlatformManager.cpp src/system/PlatformManager.h
    src/tasks/Task.cpp src/tasks/Task.h
    src/tasks/BusyWaitTask.cpp src/tasks/BusyWaitTask.h
    src/tasks/CombinedTask.cpp src/tasks/CombinedTask.h
    src/tasks/PfsReadTask.cpp src/tasks/PfsReadTask.h
    src/tasks/BurstBufferWriteTask.cpp src/tasks/BurstBufferWriteTask.h
    src/tasks/PfsWriteTask.cpp src/tasks/PfsWriteTask.h
    src/system/Sensing.cpp src/system/Sensing.h
    src/system/JobSubmitter.cpp src/system/JobSubmitter.h
    src/software/Application.cpp src/software/Application.h
    src/system/WalltimeMonitor.cpp src/system/WalltimeMonitor.h
    src/tasks/IoTask.cpp src/tasks/IoTask.h
    src/tasks/BurstBufferReadTask.cpp src/tasks/BurstBufferReadTask.h
    src/tasks/SequenceTask.cpp src/tasks/SequenceTask.h
    src/software/Phase.cpp src/software/Phase.h
    src/system/messages/SimMsg.cpp src/system/messages/SimMsg.h
    src/system/messages/SchedMsg.cpp src/system/messages/SchedMsg.h
    src/util/Configuration.cpp src/util/Configuration.h
    src/tasks/CombinedGpuTask.cpp src/tasks/CombinedGpuTask.h
    src/system/Gpu.cpp src/system/Gpu.h
    src/tasks/IdleTask.cpp src/tasks/IdleTask.h
    src/tasks/DelayTask.cpp src/tasks/DelayTask.h
    src/tasks/CombinedCpuTask.cpp src/tasks/CombinedCpuTask.h
    src/tasks/AsyncSleep.cpp src/tasks/AsyncSleep.h
    src/system/ShutdownPolicyManager.cpp src/system/ShutdownPolicyManager.h
    src/util/PerformanceTracer.cpp src/util/PerformanceTracer.h
    src/interface/NativeFifoScheduler.cpp src/interface/NativeFifoScheduler.h
    src/scheduling/utils/JobUtils.cpp src/scheduling/utils/JobUtils.h
    src/scheduling/utils/JobQueue.cpp src/scheduling/utils/JobQueue.h
    src/scheduling/utils/NodeAllocator.cpp src/scheduling/utils/NodeAllocator.h
    src/scheduling/utils/NodeCache.cpp src/scheduling/utils/NodeCache.h
    src/scheduling/base/SchedulerRegistry.cpp src/scheduling/base/SchedulerRegistry.h
    src/scheduling/schedulers/RigidFifoScheduler.cpp src/scheduling/schedulers/RigidFifoScheduler.h
    src/scheduling/schedulers/RigidBackfillScheduler.cpp src/scheduling/schedulers/RigidBackfillScheduler.h
    src/scheduling/schedulers/RigidEasyBackfillScheduler.cpp src/scheduling/schedulers/RigidEasyBackfillScheduler.h
    src/scheduling/schedulers/RigidShortestJobFirstScheduler.cpp src/scheduling/schedulers/RigidShortestJobFirstScheduler.h
    src/scheduling/schedulers/MinAgreementScheduler.cpp src/scheduling/schedulers/MinAgreementScheduler.h
    src/scheduling/schedulers/MinCommonPoolScheduler.cpp src/scheduling/schedulers/MinCommonPoolScheduler.h
    src/scheduling/schedulers/AverageCommonPoolScheduler.cpp src/scheduling/schedulers/AverageCommonPoolScheduler.h
    src/scheduling/schedulers/MinStealAgreementScheduler.cpp src/scheduling/schedulers/MinStealAgreementScheduler.h
    src/scheduling/schedulers/PrefStealAgreementScheduler.cpp src/scheduling/schedulers/PrefStealAgreementScheduler.h
    src/scheduling/schedulers/SchedulerRegistration.cpp
    src/util/EventLogger.cpp src/util/EventLogger.h
    src/scheduling/malleability/AgreementHandler.cpp src/scheduling/malleability/AgreementHandler.h
    src/scheduling/malleability/DirectAgreementHandler.cpp src/scheduling/malleability/DirectAgreementHandler.h
    src/scheduling/malleability/PoolAgreementHandler.cpp src/scheduling/malleability/PoolAgreementHandler.h
    src/scheduling/malleability/StealAgreementHandler.cpp src/scheduling/malleability/StealAgreementHandler.h
    src/scheduling/malleability/MalleabilityHelper.cpp src/scheduling/malleability/MalleabilityHelper.h
)

target_link_directories(elastisim PRIVATE ${SIMGRID_SOURCE_DIR}/lib ${SIMGRID_SOURCE_DIR}/lib64)
target_link_libraries(elastisim simgrid)
set_target_properties(elastisim PROPERTIES
    BUILD_RPATH "${SIMGRID_SOURCE_DIR}/lib;${SIMGRID_SOURCE_DIR}/lib64"
    INSTALL_RPATH "${SIMGRID_SOURCE_DIR}/lib;${SIMGRID_SOURCE_DIR}/lib64"
)
