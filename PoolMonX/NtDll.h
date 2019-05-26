#pragma once

typedef _Return_type_success_(return >= 0) LONG NTSTATUS;

enum class SystemInformationClass {
	SystemPoolTagInformation = 22,
};

struct SYSTEM_POOLTAG {
	union {
		UCHAR Tag[4];
		ULONG TagUlong;
	};
	ULONG PagedAllocs;
	ULONG PagedFrees;
	SIZE_T PagedUsed;
	ULONG NonPagedAllocs;
	ULONG NonPagedFrees;
	SIZE_T NonPagedUsed;
};

struct SYSTEM_POOLTAG_INFORMATION {
	ULONG Count;
	SYSTEM_POOLTAG TagInfo[1];
};

extern "C"
NTSTATUS NTAPI NtQuerySystemInformation(
	_In_ SystemInformationClass SystemInformationClass,
	_Out_writes_bytes_to_opt_(SystemInformationLength, *ReturnLength) PVOID SystemInformation,
	_In_ ULONG SystemInformationLength,
	_Out_opt_ PULONG ReturnLength
);
