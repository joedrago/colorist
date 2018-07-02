set(JXRLIB_SRCS
#    common/include/guiddef.h
#    common/include/wmsal.h
#    common/include/wmspecstring.h
#    common/include/wmspecstrings_adt.h
#    common/include/wmspecstrings_strict.h
#    common/include/wmspecstrings_undef.h
    image/decode/decode.c
    image/decode/decode.h
    image/decode/JXRTranscode.c
    image/decode/postprocess.c
    image/decode/segdec.c
    image/decode/strdec.c
    image/decode/strdec_x86.c
    image/decode/strInvTransform.c
    image/decode/strPredQuantDec.c
    image/encode/encode.c
    image/encode/encode.h
    image/encode/segenc.c
    image/encode/strenc.c
#    image/encode/strenc_x86.c
    image/encode/strFwdTransform.c
    image/encode/strPredQuantEnc.c
    image/sys/adapthuff.c
    image/sys/ansi.h
    image/sys/common.h
    image/sys/image.c
    image/sys/perfTimer.h
    image/sys/perfTimerANSI.c
    image/sys/strcodec.c
    image/sys/strcodec.h
    image/sys/strPredQuant.c
    image/sys/strTransform.c
    image/sys/strTransform.h
    image/sys/windowsmediaphoto.h
    image/sys/xplatform_image.h
#    image/x86/x86.h
#    jxrencoderdecoder/JXRDec420.c
#    jxrencoderdecoder/JxrDecApp.c
#    jxrencoderdecoder/JxrEnc420.c
#    jxrencoderdecoder/JxrEncApp.c
    jxrgluelib/JXRGlue.c
    jxrgluelib/JXRGlue.h
    jxrgluelib/JXRGlueJxr.c
    jxrgluelib/JXRGluePFC.c
    jxrgluelib/JXRMeta.c
    jxrgluelib/JXRMeta.h
#    jxrtestlib/JXRTest.c
#    jxrtestlib/JXRTest.h
#    jxrtestlib/JXRTestBmp.c
#    jxrtestlib/JXRTestHdr.c
#    jxrtestlib/JXRTestPnm.c
#    jxrtestlib/JXRTestTif.c
#    jxrtestlib/JXRTestYUV.c
)

add_definitions(-DDISABLE_PERF_MEASUREMENT=1)

include_directories(
    image/sys
    jxrgluelib
)

add_library(jxrlib ${JXRLIB_SRCS})
