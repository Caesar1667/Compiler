#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include "usbproc.h"
#include "opcodes.h"

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s <program.bin>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if(!fp)
    {
        perror("Failed to open binary file");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if(file_size <= 0 || (file_size % 4) != 0)
    {
        fprintf(stderr, "Error: Binary file size (%ld bytes) must be a non-zero multiple of 4 bytes.\n", file_size);
        fclose(fp);
    }

    uint8_t *send_buffer = (uint8_t *)malloc(file_size);
    if(!send_buffer)
    {
        fprintf(stderr, "Memory allocation error.\n");
        fclose(fp);
        return 1;
    }

    if(fread(send_buffer, 1, file_size, fp) != (size_t)file_size)
    {
        fprintf(stderr, "Failed to read full program file.\n");
        free(send_buffer);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    int expect_readback = 0;
    for(long i = 0; i < file_size; i += 4)
    {
        uint8_t opcode = (send_buffer[i] >> 4) & 0x0F;
        if(opcode == 0x04)
        {
            expect_readback = 1;
            break;
        }
    }

    libusb_context *ctx = NULL;
    if(libusb_init(&ctx) < 0)
    {
        fprintf(stderr, "Error: libusb_init failed\n");
        free(send_buffer);
        return 1;
    }

    libusb_device_handle *dev_handle = libusb_open_device_with_vid_pid(ctx, USB_VENDOR_ID, USB_PRODUCT_ID);
    if(!dev_handle)
    {
        fprintf(stderr, "Error: Device not found (VID: 0x%04X, PID: 0x%04X)\n", USB_VENDOR_ID, USB_PRODUCT_ID);
        libusb_exit(ctx);
        free(send_buffer);
        free(send_buffer);
        return 1;
    }

    if(libusb_kernel_driver_active(dev_handle, 0) == 1)
    {
        libusb_detach_kernel_driver(dev_handle, 0);
    }

    if(libusb_claim_interface(dev_handle, 0) < 0)
    {
        fprintf(stderr, "Error: Failed to claim interface 0\n");
        libusb_close(dev_handle);
        libusb_exit(ctx);
        free(send_buffer);
        return 1;
    }

    printf("Connected to FPGA. Sending %ld instructions (%ld bytes)...\n", file_size / 4, file_size);

    int actual_length = 0;
    int r = libusb_bulk_transfer(dev_handle, ENDPOINT_OUT, send_buffer, (int)file_size, &actual_length, TIMEOUT_MS);
    if(r < 0 || actual_length != file_size)
    {
        fprintf(stderr, "Error sending data: libusb code %d, actual sent: %d\n", r, actual_length);
    }else
    {
        printf("Transmission complete (%d bytes sent).\n", actual_length);
    }

    if(expect_readback && r == 0)
    {
        printf("Reading back %d result words (128 bytes) from FPGA...\n", COLS);
        
        int rx_size = COLS * 4;
        uint8_t rx_buffer[128];
        actual_length = 0;

        r = libusb_bulk_transfer(dev_handle, ENDPOINT_IN, rx_buffer, rx_size, &actual_length, TIMEOUT_MS);
        if(r < 0)
        {
            fprintf(stderr, "Error receiving results: libusb code %d\n", r);
        }else
        {
            printf("Received %d bytes. Unpacked Channel Outputs:\n", actual_length);
            for(int i = 0; i < actual_length; i += 4)
            {
                uint32_t raw_word = ((uint32_t)rx_buffer[i]     << 24) |
                                    ((uint32_t)rx_buffer[i + 1] << 16) |
                                    ((uint32_t)rx_buffer[i + 2] << 8)  |
                                    ((uint32_t)rx_buffer[i + 3]);

                int32_t signed_value = (int32_t)raw_word;
                printf("    Channel [%2d]: Raw = 0x%08X (%7d) | Q8.8 = %8.4f\n", i / 4, raw_word, signed_value, (double)signed_value / 256.0);
            }
        }
    }

    libusb_release_interface(dev_handle, 0);
    libusb_close(dev_handle);
    libusb_exit(ctx);
    free(send_buffer);

    return 0;
}