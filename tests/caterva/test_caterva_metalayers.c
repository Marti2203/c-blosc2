/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


CUTEST_TEST_DATA(metalayers) {
    blosc2_context *ctx;
};


CUTEST_TEST_SETUP(metalayers) {
    blosc2_init();
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.nthreads = 2;
    cparams.compcode = BLOSC_BLOSCLZ;
    data->ctx = blosc2_create_cctx(cparams);

    // Add parametrizations
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(1, 2, 4, 8));
    CUTEST_PARAMETRIZE(shapes, _test_shapes, CUTEST_DATA(
            {0, {0}, {0}, {0}}, // 0-dim
            {1, {10}, {7}, {2}}, // 1-idim
            {2, {100, 100}, {20, 20}, {10, 10}},
    ));
    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
            {true, true},
            {false, true},
    ));
}


CUTEST_TEST_TEST(metalayers) {
    CUTEST_GET_PARAMETER(shapes, _test_shapes);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);
    CUTEST_GET_PARAMETER(backend, _test_backend);

    char *urlpath = "test_metalayers.caterva";
    blosc2_remove_urlpath(urlpath);
    caterva_params_t params;
    params.itemsize = itemsize;
    params.ndim = shapes.ndim;
    for (int i = 0; i < params.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    caterva_storage_t storage = {0};
    if (backend.persistent) {
        storage.urlpath = urlpath;
    }
    storage.contiguous = backend.contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
    }
    storage.nmetalayers = 1;
    blosc2_metalayer *meta0 = &storage.metalayers[0];
    meta0->name = "test_meta";
    meta0->content_len = 3;
    double sdata0 = 5.789;
    meta0->content = (uint8_t *) &sdata0;
    meta0->content_len = sizeof(sdata0);

    /* Create original data */
    size_t buffersize = itemsize;
    for (int i = 0; i < params.ndim; ++i) {
        buffersize *= (size_t) params.shape[i];
    }

    uint8_t *buffer = malloc(buffersize);
    CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, itemsize, buffersize / itemsize));

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_TEST_ASSERT(caterva_from_buffer(data->ctx, buffer, buffersize, &params, &storage,
                                            &src));

    blosc2_metalayer vlmeta1;

    uint64_t sdata1 = 56;
    vlmeta1.name = "vlmeta1";
    vlmeta1.content = (uint8_t *) &sdata1;
    vlmeta1.content_len = sizeof(sdata1);

    CATERVA_TEST_ASSERT(blosc2_vlmeta_add(src->sc, vlmeta1.name, vlmeta1.content, vlmeta1.content_len,
                                          src->sc->storage->cparams));

    int rc = blosc2_vlmeta_exists(src->sc, "vlmeta2");
    CUTEST_ASSERT("", rc < 0);
    rc = blosc2_vlmeta_exists(src->sc, vlmeta1.name);
    CUTEST_ASSERT("", rc == 0);

    uint8_t *content;
    int32_t content_len;
    CATERVA_TEST_ASSERT(blosc2_vlmeta_get(src->sc, vlmeta1.name, &content, &content_len));
    CUTEST_ASSERT("Contents are not equal",
                  *((uint64_t *) vlmeta1.content) == *((uint64_t *) content));
    CUTEST_ASSERT("Sizes are not equal", vlmeta1.content_len == content_len);
    free(content);

    float sdata11 = 4.5f;
    vlmeta1.content = (uint8_t *) &sdata11;
    vlmeta1.content_len = sizeof(sdata11);

    CATERVA_TEST_ASSERT(blosc2_vlmeta_update(src->sc, vlmeta1.name, vlmeta1.content, vlmeta1.content_len,
                                             src->sc->storage->cparams));

    CATERVA_TEST_ASSERT(blosc2_vlmeta_get(src->sc, vlmeta1.name, &content, &content_len));
    CUTEST_ASSERT("Contents are not equal", *((float *) vlmeta1.content) == *((float *) content));
    CUTEST_ASSERT("Sizes are not equal", vlmeta1.content_len == content_len);
    free(content);

    blosc2_metalayer vlmeta2;
    vlmeta2.name = "vlmeta2";
    vlmeta2.content = (uint8_t *) &sdata1;
    vlmeta2.content_len = sizeof(sdata1);
    CATERVA_TEST_ASSERT(blosc2_vlmeta_add(src->sc, vlmeta2.name, vlmeta2.content, vlmeta2.content_len,
                                          src->sc->storage->cparams));
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));

    caterva_array_t *src2;
    caterva_open(data->ctx, urlpath, &src2);

    CATERVA_TEST_ASSERT(blosc2_vlmeta_get(src2->sc, vlmeta2.name, &content, &content_len));
    CUTEST_ASSERT("Contents are not equal", *((uint64_t *) vlmeta2.content) == *((uint64_t *) content));
    CUTEST_ASSERT("Sizes are not equal", vlmeta2.content_len == content_len);
    free(content);

    sdata0 = 1e-10;
    blosc2_metalayer meta1;
    meta1.name = meta0->name;
    meta1.content = (uint8_t *) &sdata0;
    meta1.content_len = sizeof(sdata0);

    rc  = blosc2_meta_exists(src2->sc, meta0->name);
    CUTEST_ASSERT("", rc == 1);
    CATERVA_TEST_ASSERT(blosc2_meta_update(src2->sc, meta1.name, meta1.content, meta1.content_len));

    blosc2_metalayer meta2;
    CATERVA_TEST_ASSERT(blosc2_meta_get(src2->sc, meta1.name, &meta2.content, &meta2.content_len));

    CUTEST_ASSERT("Contents are not equal", *((double *) meta2.content) == *((double *) meta1.content));
    CUTEST_ASSERT("Sizes are not equal", meta2.content_len == meta1.content_len);
    free(meta2.content);

    /* Free mallocs */
    free(buffer);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src2));
    blosc2_remove_urlpath(urlpath);
    return 0;
}


CUTEST_TEST_TEARDOWN(metalayers) {
    blosc2_free_ctx(data->ctx);
    blosc2_destroy();
}

int main() {
    CUTEST_TEST_RUN(metalayers);
}
