set(SOURCES
       cuddAddNeg.c
       cuddBddCorr.c
       cuddDecomp.c
       cuddInit.c
       cuddRead.c
       cuddSubsetSP.c
       cuddZddIsop.c
       datalimit.c
       dddmpNodeBdd.c
       safe_mem.c
       cpu_stats.c
       cuddAddWalsh.c
       cuddBddIte.c
       cuddEssent.c
       cuddInteract.c
       cuddRef.c
       cuddSymmetry.c
       cuddZddLin.c
       dddmpBinary.c
       dddmpNodeCnf.c
       mtrBasic.c
       saveimage.c
       cpu_time.c
       cuddAndAbs.c
       cuddBridge.c
       cuddExact.c
       cuddLCache.c
       cuddReorder.c
       cuddTable.c
       cuddZddMisc.c
       dddmpConvert.c
       dddmpStoreAdd.c
       mtrGroup.c
       state.c
       cuddAddAbs.c
       cuddAnneal.c
       cuddCache.c
       cuddExport.c
       cuddLevelQ.c
       cuddSat.c
       cuddUtil.c
       cuddZddPort.c
       dddmpDbg.c
       dddmpStoreBdd.c
       pathsearch.c
       st.c
       texpand.c
       cuddAddApply.c
       cuddApa.c
       cuddCheck.c
       cuddGenCof.c
       cuddLinear.c
       cuddSign.c
       cuddWindow.c
       cuddZddReord.c
       dddmpDdNodeBdd.c
       dddmpStoreCnf.c
       pipefork.c
       strsav.c
       cuddAddFind.c
       cuddAPI.c
       cuddClip.c
       cuddGenetic.c
       cuddLiteral.c
       cuddSolve.c
       cuddZddCount.c
       cuddZddSetop.c
       dddmpLoad.c
       dddmpStoreMisc.c
       prtime.c
       stub.c
       cuddAddInv.c
       cuddApprox.c
       cuddCof.c
       cuddGroup.c
       cuddMatMult.c
       cuddSplit.c
       cuddZddFuncs.c
       cuddZddSymm.c
       dddmpLoadCnf.c
       dddmpUtil.c
       ptime.c
       cuddAddIte.c
       cuddBddAbs.c
       cuddCompose.c
       cuddHarwell.c
       cuddPriority.c
       cuddSubsetHB.c
       cuddZddGroup.c
       cuddZddUtil.c
       dddmpNodeAdd.c
       epd.c
       restart.c
)

# Add include directories for CUDD
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/SVF
    ${CMAKE_CURRENT_SOURCE_DIR}/CUDD
)

#add_library(Cudd MODULE ${SOURCES})
add_library(LLVMCudd STATIC ${SOURCES})

#set_target_properties(Cudd PROPERTIES COMPILE_FLAGS "-Wno-format -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast")
set_target_properties(LLVMCudd PROPERTIES COMPILE_FLAGS "-Wno-format -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast")

# Link with LLVM libraries
llvm_map_components_to_libnames(llvm_libs 
    BitWriter 
    Core 
    IPO 
    IrReader 
    #InstCombine 
    Instrumentation 
    Target 
    Linker 
    Analysis 
    #ScalarOpts 
    Support)
# target_link_libraries(Cudd PRIVATE ${llvm_libs})
target_link_libraries(LLVMCudd PRIVATE ${llvm_libs})

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(LLVMCudd intrinsics_gen)
endif()
