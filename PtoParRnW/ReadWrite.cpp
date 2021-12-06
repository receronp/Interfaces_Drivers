// Read/Write request processors for PtoPar driver
// Copyright (C) 1999 by Walter Oney
// All rights reserved

#include "stddcls.h"
#include "driver.h"
#define S_BUSY 0x80
#define S_ACK 0x40
#define S_PAPER_END 0x20
#define S_SELECT_IN 0x10
#define S_nERROR 0x08

VOID OnCancelReadWrite(PDEVICE_OBJECT fdo, PIRP Irp);

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS DispatchCreate(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchCreate
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

	// Claim the remove lock in Win2K so that removal waits until the
	// handle closes. Don't do this in Win98, however, because this
	// device might be removed by surprise with handles open, whereupon
	// we'll deadlock in HandleRemoveDevice waiting for a close that
	// can never happen because we can't run the user-mode code that
	// would do the close.

	NTSTATUS status;
	status = IoAcquireRemoveLock(&pdx->RemoveLock, stack->FileObject);
	pdx->datoDD=0;
	if (NT_SUCCESS(status))
		{						// okay to open
		if (InterlockedIncrement(&pdx->handles) == 1)
			{					// first open handle
			}					// okay to open
		}					// first open handle
	return CompleteRequest(Irp, status, 0);
	}							// DispatchCreate

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS DispatchClose(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchClose
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	if (InterlockedDecrement(&pdx->handles) == 0)
		{						// no more open handles
		}						// no more open handles
	
	// Release the remove lock to match the acquisition done in DispatchCreate

	IoReleaseRemoveLock(&pdx->RemoveLock, stack->FileObject);

	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
	}							// DispatchClose

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS DispatchRead(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchRead
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	PUCHAR pDato = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
	unsigned char dato;
	
	//GET algorithm
	unsigned char mask_status[]={ S_BUSY, S_PAPER_END, S_SELECT_IN, S_nERROR,
								  S_BUSY, S_PAPER_END, S_SELECT_IN, S_nERROR};
	unsigned char status;
	unsigned char MASK_CTL=0x0B;
	unsigned char MASK_STS=0x80;
	unsigned char data=0;
	unsigned int i;
	unsigned int aux=0x01;
	unsigned int control;

	for( i=0; i < 8; i++, aux<<1) {
		if(i==0) {
			control=READ_PORT_UCHAR((unsigned char *)0x37A)^MASK_CTL;
			control=control&0xF7;
			WRITE_PORT_UCHAR((unsigned char *)0x37A,control^MASK_CTL);
			status=READ_PORT_UCHAR((unsigned char *)0x379)^MASK_STS;
		} else if(i==4) {
			control=READ_PORT_UCHAR((unsigned char *)0x37A)^MASK_CTL;
			control=control|0x08;
			WRITE_PORT_UCHAR((unsigned char *)0x37A,control^MASK_CTL);
			status=READ_PORT_UCHAR((unsigned char *)0x379)^MASK_STS;
		}

		if ((status&mask_status[i])!=0)
			dato=dato|aux;
	}
	
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 1;
	*pDato=pdx->datoDD;				// Simulated value.
	return STATUS_SUCCESS;
}							// DispatchRead

NTSTATUS DispatchWrite(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchWrite
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	PUCHAR pDato = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
	unsigned char dato = (unsigned char)*pDato;
	unsigned char MSK_CTRL = 0x0B;
	unsigned char control;
	unsigned char digits[] = {	0x3F, 0x06, 0x5B, 0x4F, 0x66,
								0x6D, 0x7D, 0x07, 0x7F, 0x6F};
//								0x77, 0x7C, 0x39, 0x5F, 0x79, 0x71};

	control = READ_PORT_UCHAR((unsigned char *)0x37A)^MSK_CTRL;
	control = control & 0xFE;
	WRITE_PORT_UCHAR((unsigned char *)0x37A, control^MSK_CTRL);

	if(dato / 10 == 0) {
		WRITE_PORT_UCHAR((unsigned char *)0x378, digits[dato%10]);
		pdx->datoDD = digits[dato%10];
	} else {
		WRITE_PORT_UCHAR((unsigned char *)0x378, 0xFF);
		pdx->datoDD = 0xFF;
	}

	control = control | 0x01;
	WRITE_PORT_UCHAR((unsigned char *)0x37A, control^MSK_CTRL);


	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 1;
	return STATUS_SUCCESS;
}							// DispatchWrite

#pragma LOCKEDCODE

VOID OnCancelReadWrite(IN PDEVICE_OBJECT fdo, IN PIRP Irp)
	{							// OnCancelReadWrite
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	CancelRequest(&pdx->dqReadWrite, Irp);
	}							// OnCancelReadWrite

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

VOID DpcForIsr(PKDPC Dpc, PDEVICE_OBJECT fdo, PIRP junk, PDEVICE_EXTENSION pdx)
	{							// DpcForIsr
	//PIRP nfyirp = UncacheControlRequest(pdx, &pdx->NotifyIrp);
	if (pdx->NotifyIrp)
		{					// complete notification IRP
		KdPrint((DRIVERNAME " ... Rich, DpcForIsr\n"));
		CompleteRequest(pdx->NotifyIrp, STATUS_SUCCESS, sizeof(ULONG));
		pdx->NotifyIrp=NULL;
		}					// complete notification IRP
	}							// DpcForIsr

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

BOOLEAN OnInterrupt(PKINTERRUPT InterruptObject, PDEVICE_EXTENSION pdx)
	{							// OnInterrupt
	IoRequestDpc(pdx->DeviceObject, NULL, pdx);
	return TRUE;
	}							// OnInterrupt

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS StartDevice(PDEVICE_OBJECT fdo, PCM_PARTIAL_RESOURCE_LIST raw, PCM_PARTIAL_RESOURCE_LIST translated)
	{							// StartDevice
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	NTSTATUS status;

	// Identify the I/O resources we're supposed to use.
	
	ULONG vector;
	KIRQL irql;
	KINTERRUPT_MODE mode;
	KAFFINITY affinity;
	BOOLEAN irqshare;
	BOOLEAN gotinterrupt = FALSE;

	PHYSICAL_ADDRESS portbase;
	BOOLEAN gotport = FALSE;
	UCHAR imr,control;
	
	if (!translated)
		return STATUS_DEVICE_CONFIGURATION_ERROR;		// no resources assigned??

	PCM_PARTIAL_RESOURCE_DESCRIPTOR resource = translated->PartialDescriptors;
	ULONG nres = translated->Count;
	for (ULONG i = 0; i < nres; ++i, ++resource)
		{						// for each resource
		switch (resource->Type)
			{					// switch on resource type

		case CmResourceTypePort:
			portbase = resource->u.Port.Start;
			pdx->nports = resource->u.Port.Length;
			pdx->mappedport = (resource->Flags & CM_RESOURCE_PORT_IO) == 0;
			gotport = TRUE;
			break;
	
		case CmResourceTypeInterrupt:
			irql = (KIRQL) resource->u.Interrupt.Level;
			vector = resource->u.Interrupt.Vector;
			affinity = resource->u.Interrupt.Affinity;
			mode = (resource->Flags == CM_RESOURCE_INTERRUPT_LATCHED)
				? Latched : LevelSensitive;
			irqshare = resource->ShareDisposition == CmResourceShareShared;
			gotinterrupt = TRUE;
			break;

		default:
			KdPrint((DRIVERNAME " - Unexpected I/O resource type %d\n", resource->Type));
			break;
			}					// switch on resource type
		}						// for each resource

	// Verify that we got all the resources we were expecting

	if (!(TRUE
		&& gotinterrupt
		&& gotport
		))
		{
		KdPrint((DRIVERNAME " - Didn't get expected I/O resources\n"));
		return STATUS_DEVICE_CONFIGURATION_ERROR;
		}

	if (pdx->mappedport)
		{						// map port address for RISC platform
		pdx->portbase = (PUCHAR) MmMapIoSpace(portbase, pdx->nports, MmNonCached);
		if (!pdx->mappedport)
			{
			KdPrint((DRIVERNAME " - Unable to map port range %I64X, length %X\n", portbase, pdx->nports));
			return STATUS_INSUFFICIENT_RESOURCES;
			}
		}						// map port address for RISC platform
	else
		pdx->portbase = (PUCHAR) portbase.QuadPart;

	// TODO Temporarily prevent device from interrupt if that's possible.
	// Enable device for interrupts when IoConnectInterrupt returns

	status = IoConnectInterrupt(&pdx->InterruptObject, (PKSERVICE_ROUTINE) OnInterrupt,
		(PVOID) pdx, NULL, vector, irql, irql, mode, irqshare, affinity, FALSE);
	imr=READ_PORT_UCHAR((UCHAR *)0x21);
	WRITE_PORT_UCHAR((UCHAR *)0x21,imr|0x80);
	control=READ_PORT_UCHAR((UCHAR *)0x37A);
	WRITE_PORT_UCHAR((UCHAR *)0x37A,control|0x10);
	if (!NT_SUCCESS(status))
		{
		KdPrint((DRIVERNAME " - IoConnectInterrupt failed - %X\n", status));
		if (pdx->portbase && pdx->mappedport)
			MmUnmapIoSpace(pdx->portbase, pdx->nports);
		pdx->portbase = NULL;
		return status;
		}

	return STATUS_SUCCESS;
	}							// StartDevice

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

VOID StartIo(IN PDEVICE_OBJECT fdo, IN PIRP Irp)
	{							// StartIo
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = IoAcquireRemoveLock(&pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(status))
	{
		CompleteRequest(Irp, status, 0);
		return;
		}

	// TODO Initiate read request. Be sure to set the "busy" flag before starting
	// an operation that might generate an interrupt.

	}							// StartIo

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

VOID StopDevice(IN PDEVICE_OBJECT fdo, BOOLEAN oktouch /* = FALSE */)
	{							// StopDevice
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	if (pdx->InterruptObject)
		{						// disconnect interrupt

		// TODO prevent device from generating more interrupts if possible

		IoDisconnectInterrupt(pdx->InterruptObject);
		pdx->InterruptObject = NULL;
		}						// disconnect interrupt

	if (pdx->portbase && pdx->mappedport)
		MmUnmapIoSpace(pdx->portbase, pdx->nports);
	pdx->portbase = NULL;
	}							// StopDevice
