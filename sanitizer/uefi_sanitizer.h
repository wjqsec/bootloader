EFI_ALLOCATE_POOL old_malloc;
EFI_FREE_POOL old_free;
EFI_STATUS EFIAPI my_efi_alloc_pool(IN EFI_MEMORY_TYPE PoolType, IN UINTN Size, OUT VOID **Buffer)
{
	Size += 8;
	EFI_STATUS ret = old_malloc(PoolType,Size,Buffer);
	insert_malloc_recording(*Buffer, Size);
	return ret;
	
	
}
EFI_STATUS EFIAPI my_efi_free_pool(IN VOID *Buffer)
{
	delete_malloc_recording(Buffer);
	EFI_STATUS ret = old_free(Buffer);
	return ret;
}


// old_malloc = BS->AllocatePool;
// old_free = BS->FreePool;

// BS->AllocatePool = (EFI_ALLOCATE_POOL)my_efi_alloc_pool;
// BS->FreePool = (EFI_FREE_POOL)my_efi_free_pool;
