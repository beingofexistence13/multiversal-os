/******************************************************************************
 *
 * Module Name: aeexec - Argument testing for control method execution.
 *                       Also some other miscellaneous tests.
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2023, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "aecommon.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeexec")


/******************************************************************************
 *
 * FUNCTION:    AeSetupConfiguration
 *
 * PARAMETERS:  RegionAddr          - Address for an ACPI table to be loaded
 *                                    dynamically. Test purposes only.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Call AML _CFG configuration control method
 *
 *****************************************************************************/

ACPI_STATUS
AeSetupConfiguration (
    void                    *RegionAddr)
{
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg[3];


    /*
     * Invoke _CFG method if present
     */
    ArgList.Count = 1;
    ArgList.Pointer = Arg;

    Arg[0].Type = ACPI_TYPE_INTEGER;
    Arg[0].Integer.Value = ACPI_TO_INTEGER (RegionAddr);

    (void) AcpiEvaluateObject (NULL, "\\_CFG", &ArgList, NULL);
    return (AE_OK);
}


#if (!ACPI_REDUCED_HARDWARE)
/******************************************************************************
 *
 * FUNCTION:    AfInstallGpeBlock
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Test GPE block device initialization. Requires test ASL with
 *              A \GPE2 device.
 *
 *****************************************************************************/

void
AfInstallGpeBlock (
    void)
{
    ACPI_STATUS                 Status;
    ACPI_HANDLE                 Handle;
    ACPI_GENERIC_ADDRESS        BlockAddress;
    ACPI_HANDLE                 GpeDevice;
    ACPI_OBJECT_TYPE            Type;


    /* _GPE should always exist */

    Status = AcpiGetHandle (NULL, "\\_GPE", &Handle);
    ACPI_CHECK_OK (AcpiGetHandle, Status);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    memset (&BlockAddress, 0, sizeof (ACPI_GENERIC_ADDRESS));
    BlockAddress.SpaceId = ACPI_ADR_SPACE_SYSTEM_MEMORY;
    BlockAddress.Address = 0x76540000;

    /* Attempt to install a GPE block on GPE2 (if present) */

    Status = AcpiGetHandle (NULL, "\\GPE2", &Handle);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiGetType (Handle, &Type);
        if (ACPI_FAILURE (Status) ||
           (Type != ACPI_TYPE_DEVICE))
        {
            return;
        }

        Status = AcpiInstallGpeBlock (Handle, &BlockAddress, 7, 8);
        ACPI_CHECK_OK (AcpiInstallGpeBlock, Status);

        Status = AcpiInstallGpeHandler (Handle, 8,
            ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
        ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

        Status = AcpiEnableGpe (Handle, 8);
        ACPI_CHECK_OK (AcpiEnableGpe, Status);

        Status = AcpiGetGpeDevice (0x30, &GpeDevice);
        ACPI_CHECK_OK (AcpiGetGpeDevice, Status);

        Status = AcpiGetGpeDevice (0x42, &GpeDevice);
        ACPI_CHECK_OK (AcpiGetGpeDevice, Status);

        Status = AcpiGetGpeDevice (AcpiCurrentGpeCount-1, &GpeDevice);
        ACPI_CHECK_OK (AcpiGetGpeDevice, Status);

        Status = AcpiGetGpeDevice (AcpiCurrentGpeCount, &GpeDevice);
        ACPI_CHECK_STATUS (AcpiGetGpeDevice, Status, AE_NOT_EXIST);

        Status = AcpiRemoveGpeHandler (Handle, 8, AeGpeHandler);
        ACPI_CHECK_OK (AcpiRemoveGpeHandler, Status);
    }

    /* Attempt to install a GPE block on GPE3 (if present) */

    Status = AcpiGetHandle (NULL, "\\GPE3", &Handle);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiGetType (Handle, &Type);
        if (ACPI_FAILURE (Status) ||
           (Type != ACPI_TYPE_DEVICE))
        {
            return;
        }

        Status = AcpiInstallGpeBlock (Handle, &BlockAddress, 8, 11);
        ACPI_CHECK_OK (AcpiInstallGpeBlock, Status);
    }
}
#endif /* !ACPI_REDUCED_HARDWARE */


/******************************************************************************
 *
 * FUNCTION:    AeTestBufferArgument
 *
 * DESCRIPTION: Test using a Buffer object as a method argument
 *
 *****************************************************************************/

void
AeTestBufferArgument (
    void)
{
    ACPI_OBJECT_LIST        Params;
    ACPI_OBJECT             BufArg;
    UINT8                   Buffer[] =
    {
        0,0,0,0,
        4,0,0,0,
        1,2,3,4
    };


    BufArg.Type = ACPI_TYPE_BUFFER;
    BufArg.Buffer.Length = 12;
    BufArg.Buffer.Pointer = Buffer;

    Params.Count = 1;
    Params.Pointer = &BufArg;

    (void) AcpiEvaluateObject (NULL, "\\BUF", &Params, NULL);
}


static ACPI_OBJECT                 PkgArg;
static ACPI_OBJECT                 PkgElements[5];
static ACPI_OBJECT                 Pkg2Elements[5];
static ACPI_OBJECT_LIST            Params;

/******************************************************************************
 *
 * FUNCTION:    AeTestPackageArgument
 *
 * DESCRIPTION: Test using a Package object as a method argument
 *
 *****************************************************************************/

void
AeTestPackageArgument (
    void)
{

    /* Main package */

    PkgArg.Type = ACPI_TYPE_PACKAGE;
    PkgArg.Package.Count = 4;
    PkgArg.Package.Elements = PkgElements;

    /* Main package elements */

    PkgElements[0].Type = ACPI_TYPE_INTEGER;
    PkgElements[0].Integer.Value = 0x22228888;

    PkgElements[1].Type = ACPI_TYPE_STRING;
    PkgElements[1].String.Length = sizeof ("Top-level package");
    PkgElements[1].String.Pointer = "Top-level package";

    PkgElements[2].Type = ACPI_TYPE_BUFFER;
    PkgElements[2].Buffer.Length = sizeof ("XXXX");
    PkgElements[2].Buffer.Pointer = (UINT8 *) "XXXX";

    PkgElements[3].Type = ACPI_TYPE_PACKAGE;
    PkgElements[3].Package.Count = 2;
    PkgElements[3].Package.Elements = Pkg2Elements;

    /* Subpackage elements */

    Pkg2Elements[0].Type = ACPI_TYPE_INTEGER;
    Pkg2Elements[0].Integer.Value = 0xAAAABBBB;

    Pkg2Elements[1].Type = ACPI_TYPE_STRING;
    Pkg2Elements[1].String.Length = sizeof ("Nested Package");
    Pkg2Elements[1].String.Pointer = "Nested Package";

    /* Parameter object */

    Params.Count = 1;
    Params.Pointer = &PkgArg;

    (void) AcpiEvaluateObject (NULL, "\\_PKG", &Params, NULL);
}


/******************************************************************************
 *
 * FUNCTION:    AeGetDevices
 *
 * DESCRIPTION: Stubbed at this time.
 *
 *****************************************************************************/

ACPI_STATUS
AeGetDevices (
    ACPI_HANDLE                     ObjHandle,
    UINT32                          NestingLevel,
    void                            *Context,
    void                            **ReturnValue)
{

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    ExecuteOSI
 *
 * PARAMETERS:  OsiString           - String passed to _OSI method
 *              ExpectedResult      - 0 (FALSE) or ACPI_UINT64_MAX (TRUE)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute the internally implemented (in ACPICA) _OSI method.
 *
 *****************************************************************************/

ACPI_STATUS
ExecuteOSI (
    char                    *OsiString,
    UINT64                  ExpectedResult)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg[1];
    ACPI_BUFFER             ReturnValue;
    ACPI_OBJECT             *Obj;


    /* Setup input argument */

    ArgList.Count = 1;
    ArgList.Pointer = Arg;

    Arg[0].Type = ACPI_TYPE_STRING;
    Arg[0].String.Pointer = OsiString;
    Arg[0].String.Length = strlen (Arg[0].String.Pointer);

    /* Ask ACPICA to allocate space for the return object */

    ReturnValue.Length = ACPI_ALLOCATE_BUFFER;

    Status = AcpiEvaluateObject (NULL, "\\_OSI", &ArgList, &ReturnValue);

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf (
            "Could not execute _OSI method, %s\n",
            AcpiFormatException (Status));
        return (Status);
    }

    Status = AE_ERROR;

    if (ReturnValue.Length < sizeof (ACPI_OBJECT))
    {
        AcpiOsPrintf (
            "Return value from _OSI method too small, %.8X\n",
            (UINT32) ReturnValue.Length);
        goto ErrorExit;
    }

    Obj = ReturnValue.Pointer;
    if (Obj->Type != ACPI_TYPE_INTEGER)
    {
        AcpiOsPrintf (
            "Invalid return type from _OSI method, %.2X\n", Obj->Type);
        goto ErrorExit;
    }

    if (Obj->Integer.Value != ExpectedResult)
    {
        AcpiOsPrintf (
            "Invalid return value from _OSI, expected %8.8X%8.8X found %8.8X%8.8X\n",
            ACPI_FORMAT_UINT64 (ExpectedResult),
            ACPI_FORMAT_UINT64 (Obj->Integer.Value));
        goto ErrorExit;
    }

    Status = AE_OK;

    /* Reset the OSI data */

    AcpiGbl_OsiData = 0;

ErrorExit:

    /* Free a buffer created via ACPI_ALLOCATE_BUFFER */

    AcpiOsFree (ReturnValue.Pointer);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AeGenericRegisters
 *
 * DESCRIPTION: Call the AcpiRead/Write interfaces.
 *
 *****************************************************************************/

static ACPI_GENERIC_ADDRESS       GenericRegister;

void
AeGenericRegisters (
    void)
{
    ACPI_STATUS             Status;
    UINT64                  Value;


    GenericRegister.Address = 0x1234;
    GenericRegister.BitWidth = 64;
    GenericRegister.BitOffset = 0;
    GenericRegister.SpaceId = ACPI_ADR_SPACE_SYSTEM_IO;

    Status = AcpiRead (&Value, &GenericRegister);
    ACPI_CHECK_OK (AcpiRead, Status);

    Status = AcpiWrite (Value, &GenericRegister);
    ACPI_CHECK_OK (AcpiWrite, Status);

    GenericRegister.Address = 0x12345678;
    GenericRegister.BitOffset = 0;
    GenericRegister.SpaceId = ACPI_ADR_SPACE_SYSTEM_MEMORY;

    Status = AcpiRead (&Value, &GenericRegister);
    ACPI_CHECK_OK (AcpiRead, Status);

    Status = AcpiWrite (Value, &GenericRegister);
    ACPI_CHECK_OK (AcpiWrite, Status);
}
