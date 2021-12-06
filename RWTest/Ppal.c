#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

HANDLE *device;
unsigned char dato=0x55;
DWORD aux;

void main() {
	printf("Abriendo comunicación con el dispositivo ... \n");
	device=CreateFile("\\\\.\\PTOPARRnW", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (device!=INVALID_HANDLE_VALUE) {
		printf(" ... comunicación establecida!\n");
		printf("Dame el dato: ");
		scanf("%d",&dato);
		WriteFile(device,&dato,1,&aux,NULL);
		printf("Write: dato=%02x, aux=%02x\n",dato,aux);
		ReadFile(device,&dato,1,&aux,NULL);
		printf("Read: dato=%02x, aux=%02x\n",dato,aux);
		CloseHandle(device);
	}
	printf("Fin\n");
	getchar();
}