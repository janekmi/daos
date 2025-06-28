#define DB_PATH_TEMPLATE "/tmp/dlck_XXXXXX"

int   Tgt_id            = 0;
void *TLSS[XSTREAM_MAX] = {NULL};

struct vos_tls *
dlck_tls_get()
{
	return TLSS[Tgt_id];
}

struct bio_xs_context *
dlck_xsctx_get()
{
	return XSCTXS[Tgt_id];
}

// void *
// thread_function(void *arg)
// {
// 	struct dlck_array da  = {0};
// 	struct dlck_stats ds  = {0};
// 	daos_handle_t    *coh = (daos_handle_t *)arg;
// 	int               rc;

// 	dlck_array_init(sizeof(struct dlck_dtx_rec), 10, &da);

// 	TLSS[0] = vos_standalone_tls_alloc(DAOS_TGT_TAG);

// 	rc = dlck_vos_cont_rec_get_active(*coh, &da, &ds);
// 	assert(rc == 0);

// 	printf("dda.touched = %u\n", ds.touched);

// 	vos_standalone_tls_free(TLSS[0]);

// 	return NULL;
// }

{
    /** detach global state */

    // vos_tls_getter_set(dlck_tls_get, &TLSS[XSTREAM_SYS]);
    // vos_xsctxt_getter_set(dlck_xsctx_get, &XSCTXS[XSTREAM_SYS]);

    // db_path = temp_db_path_init();
    // if (db_path == NULL) {
    // 	return -1;
    // }

    // rc = vos_self_init(db_path, true, tgt_id);
    // if (rc) {
    // 	D_ERROR("Error initializing VOS instance");
    // 	return rc;
    // }

    // bio_nvme_fini();

    // rc = bio_nvme_init(args->files[0], 0, 5120, 2, 1, false);
    // if (rc != 0) {
    // 	return rc;
    // }

    // rc = bio_xsctxt_alloc(&XSCTX, BIO_SYS_TGT_ID, true);
    // if (rc) {
    // 	return rc;
    // }

    // void bio_xsctxt_free(struct bio_xs_context *ctxt);

    // vos_xsctxt_getter_set(dlck_xsctx_get);

    // rc = uuid_parse(pool_uuid, uuid);
    // if (rc != 0) {
    // 	return rc;
    // }

    // rc = vos_pool_open(args->files[0], uuid, flags, &poh);
    // const char *path = "/tmp/vos-1";
    // const char *path = "/mnt/daos/20d1ef00-c89b-4f10-9fdb-31b9b086a0b7/vos-1";

    // rc = dlck_recreate(args->files[0], uuid);
    // if (rc != 0) {
    // 	return rc;
    // }
    // (void) dlck_recreate;

    // rc = vos_pool_open(args->files[0], uuid, flags, &poh0);
    // assert(rc == 0);

    /** XXX */
    // rc = uuid_parse(cont2_uuid, uuid);
    // assert(rc == 0);

    // rc = vos_cont_create(poh, uuid);
    // assert(rc == 0);
    /** XXX */

    // xxx(poh0);

    /** -- */

    // TLSS[1] = vos_standalone_tls_alloc(DAOS_TGT_TAG);
    // assert(TLSS[1] != NULL);

    // rc = bio_xsctxt_alloc(&XSCTXS[1], 1, true);
    // if (rc) {
    // 	return rc;
    // }

    // Tgt_id = 1;

    // rc = dlck_recreate(args->files[1], uuid);
    // if (rc != 0) {
    // 	return rc;
    // }
    // (void) dlck_recreate;

    // rc = vos_pool_open(args->files[1], uuid, flags, &poh1);
    // assert(rc == 0);

    // xxx(poh1);

    // Tgt_id = 0;
    // xxx(poh0);

    // Tgt_id = 1;
    // xxx(poh1);

    // assert(uuid_is_null(args.common.co_uuid) == 0);

    // rc = vos_cont_open(poh, args.common.co_uuid, &coh);
    // assert(rc == 0);

    // vos_tls_getter_set(dlck_tls_get);

    // pthread_t thread;

    // if (pthread_create(&thread, NULL, thread_function, &coh) != 0) {
    // 	perror("Failed to create thread");
    // 	return 1;
    // }

    // Wait for the thread to complete
    // pthread_join(thread, NULL);

    // struct dlck_dtx_rec_array dda = {0};

    // rc = dlck_vos_cont_rec_get_active(coh, &dda);
    // assert(rc == 0);

    // printf("dda.touched = %u\n", dda.touched);
};
