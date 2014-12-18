// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <stdio.h>
#include "pothos_zynq_dma_driver.h"

int test(const int index)
{
    int ret = 0;
    printf("Begin pothos axi stream userspace test %d\n", index);

    /////////////////////////// init ///////////////////////////
    printf("Create DMA channels\n");
    pzdud_t *s2mm = pzdud_create(index, PZDUD_S2MM);
    if (s2mm == NULL) return EXIT_FAILURE;

    pzdud_t *mm2s = pzdud_create(index, PZDUD_MM2S);
    if (mm2s == NULL) return EXIT_FAILURE;

    /////////////////////////// allocate ///////////////////////////
    printf("Allocate DMA channels\n");
    ret = pzdud_alloc(s2mm, 4, 4096);
    printf("pzdud_alloc(s2mm) %d\n", ret);
    if (ret != PZDUD_OK) return EXIT_FAILURE;

    ret = pzdud_alloc(mm2s, 4, 4096);
    printf("pzdud_alloc(mm2s) %d\n", ret);
    if (ret != PZDUD_OK) return EXIT_FAILURE;

    /////////////////////////// init ///////////////////////////
    ret = pzdud_init(s2mm, true);
    printf("pzdud_init(s2mm) %d\n", ret);
    if (ret != PZDUD_OK) return EXIT_FAILURE;

    ret = pzdud_init(mm2s, true);
    printf("pzdud_init(mm2s) %d\n", ret);
    if (ret != PZDUD_OK) return EXIT_FAILURE;

    ////////////////////////// loopback ////////////////////////

    //expect a timeout here
    ret = pzdud_wait(s2mm, 100);
    if (ret != PZDUD_ERROR_TIMEOUT)
    {
        printf("Fail pzdud_wait(s2mm) %d\n", ret);
        return EXIT_FAILURE;
    }

    //dont expect a timeout here
    ret = pzdud_wait(mm2s, 100);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_wait(mm2s) %d\n", ret);
        return EXIT_FAILURE;
    }

    size_t len;
    int handle = pzdud_acquire(mm2s, &len);
    if (handle < 0)
    {
        printf("Fail pzdud_acquire(mm2s) %d\n", handle);
        return EXIT_FAILURE;
    }
    printf("available %zu bytes\n", len);
    pzdud_release(mm2s, handle, 64);

    sleep(1);
    handle = pzdud_acquire(s2mm, &len);
    if (handle < 0)
    {
        printf("Fail pzdud_acquire(s2mm) %d\n", handle);
        return EXIT_FAILURE;
    }
    printf("recv %zu bytes\n", len);
    pzdud_release(s2mm, handle, 0);

    /////////////////////////// halt ///////////////////////////
    ret = pzdud_halt(s2mm);
    printf("pzdud_halt(s2mm) %d\n", ret);
    if (ret != PZDUD_OK) return EXIT_FAILURE;

    ret = pzdud_halt(mm2s);
    printf("pzdud_halt(mm2s) %d\n", ret);
    if (ret != PZDUD_OK) return EXIT_FAILURE;

    /////////////////////////// free ///////////////////////////
    printf("Free DMA channels\n");
    pzdud_free(s2mm);
    pzdud_free(mm2s);

    /////////////////////////// cleanup ///////////////////////////
    printf("Destroy DMA channels\n");
    pzdud_destroy(s2mm);
    pzdud_destroy(mm2s);

/*
    ////////////////// init /////////////////
    pzdud_t *dma = pzdud_create(index);
    if (dma == NULL)
    {
        printf("Fail pzdud_create\n");
        return EXIT_FAILURE;
    }

    ret = pzdud_reset(dma);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_reset %d\n", ret);
        return EXIT_FAILURE;
    }

    ////////////////// alloc /////////////////
    ret = pzdud_alloc(dma, PZDUD_S2MM, 4, 8192);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_alloc %d\n", ret);
        return EXIT_FAILURE;
    }
    ret = pzdud_alloc(dma, PZDUD_MM2S, 4, 8192);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_alloc %d\n", ret);
        return EXIT_FAILURE;
    }

    ////////////// init engine ///////////////
    ret = pzdud_init(dma, PZDUD_S2MM, true);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_init %d\n", ret);
        return EXIT_FAILURE;
    }
    ret = pzdud_init(dma, PZDUD_MM2S, true);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_init %d\n", ret);
        return EXIT_FAILURE;
    }

    /////////////// test loopback ////////////////

    //expect a timeout here
    ret = pzdud_wait(dma, PZDUD_S2MM, 100);
    if (ret != PZDUD_ERROR_TIMEOUT)
    {
        printf("Fail pzdud_wait(s2mm) %d\n", ret);
        return EXIT_FAILURE;
    }

    //dont expect a timeout here
    ret = pzdud_wait(dma, PZDUD_MM2S, 100);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_wait(mm2s) %d\n", ret);
        return EXIT_FAILURE;
    }

    size_t len;
    int handle = pzdud_acquire(dma, PZDUD_MM2S, &len);
    if (handle < 0)
    {
        printf("Fail pzdud_acquire(mm2s) %d\n", handle);
        return EXIT_FAILURE;
    }
    printf("available %zu bytes\n", len);
    pzdud_release(dma, PZDUD_MM2S, handle, 64);

    sleep(1);
    handle = pzdud_acquire(dma, PZDUD_S2MM, &len);
    if (handle < 0)
    {
        printf("Fail pzdud_acquire(s2mm) %d\n", handle);
        return EXIT_FAILURE;
    }
    printf("recv %zu bytes\n", len);
    pzdud_release(dma, PZDUD_S2MM, handle, 0);

    /////////////// halt engine /////////////
    ret = pzdud_halt(dma, PZDUD_S2MM);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_halt %d\n", ret);
        return EXIT_FAILURE;
    }
    ret = pzdud_halt(dma, PZDUD_MM2S);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_halt %d\n", ret);
        return EXIT_FAILURE;
    }

    ////////////////// free /////////////////
    ret = pzdud_free(dma, PZDUD_S2MM);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_free %d\n", ret);
        return EXIT_FAILURE;
    }
    ret = pzdud_free(dma, PZDUD_MM2S);
    if (ret != PZDUD_OK)
    {
        printf("Fail pzdud_free %d\n", ret);
        return EXIT_FAILURE;
    }

    ////////////////// cleanup /////////////////
    pzdud_destroy(dma);
    */
    printf("Done!\n");

    return EXIT_SUCCESS;
}

int main(int argc, const char* argv[])
{
    if (test(0) != 0) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
