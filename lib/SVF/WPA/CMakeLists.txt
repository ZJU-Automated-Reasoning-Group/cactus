include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/SVF
)

# Need to specify paths relative to the parent directory
set(WPA_SOURCES
    WPA/Andersen.cpp
    WPA/AndersenLCD.cpp
    WPA/AndersenStat.cpp
    WPA/AndersenWave.cpp
    WPA/AndersenWaveDiff.cpp
    WPA/FlowSensitive.cpp
    WPA/FlowSensitiveStat.cpp
    WPA/WPAPass.cpp
    # DSA-related files
    WPA/Basic.cpp
    WPA/DataStructure.cpp
    WPA/DSGraph.cpp
    WPA/DSNodeEquivs.cpp
    WPA/Local.cpp
    WPA/BottomUpClosure.cpp
    WPA/TopDownClosure.cpp
    WPA/CompleteBottomUp.cpp
    WPA/DSCallGraph.cpp
    WPA/DataStructureCallGraph.cpp
    WPA/DataStructureStats.cpp
    WPA/Printer.cpp
    WPA/DSAExternals.cpp
)

# Add WPA sources to the parent directory's SOURCES variable
set(SOURCES ${SOURCES} ${WPA_SOURCES} PARENT_SCOPE) 