; The 32-bit import slots of the functions of Windows 8 the toolset's C++ library calls directly (Win7Imports.cpp):
; their decorated names hold an '@', which C++ cannot spell, so they are defined here over the functions there, in
; the read-only data as the system's slots are.

.386
.model flat

EXTERN _oj_CreateFile2@20:PROC
EXTERN _oj_CopyFile2@12:PROC
EXTERN _oj_GetSystemTimePreciseAsFileTime@4:PROC

PUBLIC __imp__CreateFile2@20
PUBLIC __imp__CopyFile2@12
PUBLIC __imp__GetSystemTimePreciseAsFileTime@4

.const
__imp__CreateFile2@20 DD _oj_CreateFile2@20
__imp__CopyFile2@12 DD _oj_CopyFile2@12
__imp__GetSystemTimePreciseAsFileTime@4 DD _oj_GetSystemTimePreciseAsFileTime@4

END
