#include "did_wallet.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "leveldb/c.h"
#include "common/aes.h"
#include "cJSON/cJSON.h"

#define MAX_PATH (260)
#define DID_KEY_PREFIX "did_"

typedef struct wallet_context_tag
{
    char path[MAX_PATH];
    leveldb_t *db;
} wallet_context_t;

wallet_handle wallet_handle_create(const char *name, const char *path)
{
    leveldb_options_t *options = NULL;
    char *error = NULL;
    wallet_context_t *context = (wallet_context_t *)malloc(sizeof(wallet_context_t));
    if (context == NULL)
    {
        return NULL;
    }

    strcpy(context->path, path);

    options = leveldb_options_create();
    if (options == NULL)
    {
        free(context);
        return NULL;
    }

    leveldb_options_set_create_if_missing(options, 1);
    context->db = leveldb_open(options, path, &error);
    if (context->db == NULL)
    {
        leveldb_options_destroy(options);
        free(context);
        return NULL;
    }

    leveldb_options_destroy(options);
    return context;
}

void wallet_handle_destroy(wallet_handle handle)
{
    wallet_context_t *context = (wallet_context_t *)handle;
    if (context->db != NULL)
    {
        leveldb_close(context->db);
    }

    context->db = NULL;

    free(context);
}

int wallet_store_did(wallet_handle wallet, did_handle did, const char *name, const char *password)
{
    wallet_context_t *context = (wallet_context_t *)wallet;
    int len = did_serialize(did, NULL, 0);
    if (len <= 0)
    {
        return len;
    }

    // Padding buffer with 0 for aes encrypt
    len = (len + 15) / 16 * 16;
    char *buffer = (char *)malloc(len);
    char key[128] = {0};
    char *error = NULL;
    struct AES_ctx aes_ctx;

    AES_init_ctx(&aes_ctx, (const uint8_t *)password);
    AES_ctx_set_iv(&aes_ctx, (const uint8_t *)password);
    memset(buffer, 0, len);
    did_serialize(did, buffer, len);

    AES_CBC_encrypt_buffer(&aes_ctx, (unsigned char *)buffer, len);

    sprintf(key, "%s%s", DID_KEY_PREFIX, name);

    leveldb_writeoptions_t *options = leveldb_writeoptions_create();

    leveldb_put(context->db, options, key, strlen(key), buffer, len, &error);

    free(buffer);
    leveldb_writeoptions_destroy(options);

    return len;
}

did_handle wallet_load_did(wallet_handle wallet, const char *name, const char *password)
{
    wallet_context_t *context = (wallet_context_t *)wallet;
    char key[128] = {0};
    char *error = NULL;
    size_t len = 0;
    struct AES_ctx aes_ctx;

    AES_init_ctx(&aes_ctx, (const unsigned char *)password);
    AES_ctx_set_iv(&aes_ctx, (const unsigned char *)password);

    sprintf(key, "%s%s", DID_KEY_PREFIX, name);

    leveldb_readoptions_t *options = leveldb_readoptions_create();
    char *data = leveldb_get(context->db, options, key, strlen(key), &len, &error);

    if (data == NULL)
    {
        free(options);
        return NULL;
    }

    AES_CBC_decrypt_buffer(&aes_ctx, (unsigned char *)data, len);

    // Padding with 0 has no effect on parsing as json
    did_handle did = did_deserialize(data);

    leveldb_readoptions_destroy(options);
    free(data);
    return  = 