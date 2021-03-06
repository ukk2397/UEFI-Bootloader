#include "uefibootloader.h"

const char timeout = 5;
const unsigned char no_of_os_loaders = 6;
const OS_LOADER_ENTRY_POINT Loaders[] =
        {
			{ 
				.path=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi", 
				.label=L"Microsoft Windows" 
			}, 
        	{ 
				.path=L"\\EFI\\ubuntu\\grubx64.efi", 
				.label=L"Linux Ubuntu" 
			}, 
        	{ 
				.path=L"\\EFI\\debian\\grubx64.efi", 
				.label=L"Linux Debian" 
			}, 
        	{ 
				.path=L"\\EFI\\fedora\\grubx64.efi", 
				.label=L"Linux Fedora" 
			},
        	{ 
				.path=L"\\System\\Library\\CoreServices\\boot.efi", 
				.label=L"Mac OS" 
			},
        	{ 
				.path=L"\\shellx64.efi", 
				.label=L"EFI Shell" 
			}
		};
        

unsigned int FETCH_ENTRIES(OS_ENTRY_RECORD* os, unsigned char firstKey);
EFI_STATUS FETCH_KEY_PRESS(UINT64* key);
EFI_STATUS OS_MENU_ENTRY_CALL(OS_ENTRY_RECORD* os, unsigned int key);
EFI_STATUS LOAD_OPERATING_SYSTEM(OS_ENTRY_RECORD sys);
EFI_STATUS CLOSE_BOOTLOADER();
EFI_STATUS CLEAR_CONSOLE();

VOID EFIAPI CALL_BACK_TIMER(EFI_EVENT event, void* context);


EFI_STATUS EFIAPI UefiMain (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable)
{
    IMAGE_HANDLER_GLOBAL = ImageHandle;
    SYSTEM_TABLE_GLOBAL = SystemTable;
    BOOT_SERVICE_GLOBAL = SYSTEM_TABLE_GLOBAL->BootServices;
    
    EFI_STATUS err;
    
    err = CLEAR_CONSOLE();
    if (err == EFI_ACCESS_DENIED || err == EFI_SECURITY_VIOLATION) 
    {   
        Print(L"Error Occured. Unable to Clean Console!\n");
    }
    
    const CHAR16 LOGO[][72] =
        {
			{L"\n"},
			{L"U   B   L\n"}, 
	        {L"E   O   OA\n"},
        	{L"F   O   DE\n"},
       	 	{L"I   T   R\n"},
        	{L"\n"}
	};
    unsigned char ch;
    for(ch = 0; ch < 6; ++ch)
    {
        Print(LOGO[ch]);
    }
    Print(L"\tUEFI Bootloader\n");
            
    OS_ENTRY_RECORD os[20];
    unsigned int num_of_menu_entries = 1;
    
    unsigned int numOfLoaders = FETCH_ENTRIES(os, num_of_menu_entries);
    num_of_menu_entries += numOfLoaders;
    
    unsigned short int i;
    Print(L"0 - exit\n");
    for (i = 1; i< num_of_menu_entries; i++)
    {
        Print(L"%d - %s on filesystem with label \"%s\" and size %dMB \n", i, os[i].name, os[i].label, os[i].size>>20);
    }
    
    EFI_EVENT timer;
    if(num_of_menu_entries > 1)
    {
        CONTEXT_CALLBACK context = {timeout, num_of_menu_entries, os};
        
        err = BOOT_SERVICE_GLOBAL->CreateEventEx(EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, &CALL_BACK_TIMER, &context, NULL, &timer);
        if(err == EFI_INVALID_PARAMETER)    
            Print(L"Invalid parameters for timer creation\n");        
         
        err = BOOT_SERVICE_GLOBAL->SetTimer(timer, TimerPeriodic, EFI_TIMER_DURATION(1));
        if (err==EFI_INVALID_PARAMETER)
            Print(L"Invalid parameters for SetTimer\n");
    }
    else
    {
        Print(L"No operating systems found!\n");
    }
    
    UINT64 key;
    do 
    {
        err = FETCH_KEY_PRESS(&key);
        BOOT_SERVICE_GLOBAL->CloseEvent(timer);
        if (err == EFI_ACCESS_DENIED || err == EFI_SECURITY_VIOLATION) 
        {   
            Print(L"Key read error!\n");    
        }
        
    } while (!(key-48 >=0 && key-48 < num_of_menu_entries));
    
    if(key-48 == 0)
        err = CLOSE_BOOTLOADER();
    else
        err = OS_MENU_ENTRY_CALL(os, key-48);
    
    if (err == EFI_ACCESS_DENIED || err == EFI_SECURITY_VIOLATION) 
    {   
        Print(L"Operating System Loader file Access Error!\n");  
    }

    return EFI_SUCCESS;

}

EFI_STATUS CLEAR_CONSOLE()
{
    return SYSTEM_TABLE_GLOBAL->ConOut->ClearScreen(SYSTEM_TABLE_GLOBAL->ConOut);
}

EFI_STATUS FETCH_KEY_PRESS(UINT64* key)
{
    UINTN index;
    EFI_INPUT_KEY k;
    EFI_STATUS err;
    BOOT_SERVICE_GLOBAL->WaitForEvent(1, &SYSTEM_TABLE_GLOBAL->ConIn->WaitForKey, &index);

    err = SYSTEM_TABLE_GLOBAL->ConIn->ReadKeyStroke(SYSTEM_TABLE_GLOBAL->ConIn, &k);
    if (EFI_ERROR(err))
        return err;

    *key = KEY_INTERRUPT(0, k.ScanCode, k.UnicodeChar);
    return EFI_SUCCESS;
}

VOID EFIAPI CALL_BACK_TIMER(EFI_EVENT event, void* context)
{
    CONTEXT_CALLBACK* ctx = (CONTEXT_CALLBACK*)context;
    Print(L"%d...", ctx->timeout--);
    if(ctx->timeout < 0)
    {
        EFI_STATUS err;
        Print(L"\n");
        if(ctx->entries > 1)
        {
            err = OS_MENU_ENTRY_CALL(ctx->systems, 1);
            if (err == EFI_ACCESS_DENIED || err == EFI_SECURITY_VIOLATION) 
            {   
                Print(L"Loader file access error!\n");  
            }
        }
        else
        {
            err = CLOSE_BOOTLOADER();
        }
    }
}

EFI_STATUS OS_MENU_ENTRY_CALL(OS_ENTRY_RECORD* os, unsigned int key)
{
    Print(L"\nLoading %s", os[key].name);
    return LOAD_OPERATING_SYSTEM(os[key]);
}

EFI_STATUS CLOSE_BOOTLOADER()
{
    Print(L"Exiting EFI BootLoader");
    return EFI_SUCCESS;
}

EFI_STATUS LOAD_OPERATING_SYSTEM(OS_ENTRY_RECORD system)
{
    EFI_HANDLE image;
    EFI_DEVICE_PATH* path;
    EFI_STATUS error;
    path = FileDevicePath(system.device, system.path);    
    BOOT_SERVICE_GLOBAL->LoadImage(FALSE, IMAGE_HANDLER_GLOBAL, path, NULL, 0, &image);
    
    error = CLEAR_CONSOLE();
    if (error == EFI_ACCESS_DENIED || error == EFI_SECURITY_VIOLATION) 
    {   
        Print(L"Unable to clear screen!\n");
    }
    
    error = BOOT_SERVICE_GLOBAL->StartImage(image, NULL, NULL);  

    return error;
}

unsigned int FETCH_ENTRIES(OS_ENTRY_RECORD* os, unsigned char firstKey)
{
    unsigned int num = 0;
    EFI_GUID guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID fsi_guid = EFI_FILE_SYSTEM_INFO_ID;
    UINTN length = 1000;
    EFI_HANDLE devices[length];
    BOOT_SERVICE_GLOBAL->LocateHandle(2, &guid, 0, &length, devices);
    length = length/sizeof(EFI_HANDLE);
    unsigned int i;
    for (i = 0; i < length; ++i)
    {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
        EFI_FILE_PROTOCOL* root;
	
		UINTN buffer_size = 200;
		UINTN *buffer[buffer_size];
        
		BOOT_SERVICE_GLOBAL->HandleProtocol(devices[i], &guid, (void **) &fs);
        fs->OpenVolume(fs, &root);
		
		EFI_STATUS deviceInfoTaken = root->GetInfo(root,&fsi_guid, &buffer_size,(void *) buffer);
		EFI_FILE_PROTOCOL* file;
        
		unsigned int j;
        for(j = 0; j < no_of_os_loaders; ++j)
        {
            if(root->Open(root,&file, Loaders[j].path, EFI_FILE_MODE_READ, 0ULL) == EFI_SUCCESS)
            {
                file->Close(file);
				CHAR16 * label=L"<no label>";
				UINT64 size=0;
				if (deviceInfoTaken == EFI_SUCCESS)
				{
					EFI_FILE_SYSTEM_INFO * fi = (EFI_FILE_SYSTEM_INFO *) buffer;
					size = fi->VolumeSize;
					label = fi->VolumeLabel;
				}
                
				OS_ENTRY_RECORD sys = 	{
											.device=devices[i], 
											.path = Loaders[j].path, 
											.name = Loaders[j].label, 
											.label=label, 
											.size=size
									 	};
                os[firstKey+num] = sys;
                num++;
            }
        }
    }
    return num;
}
