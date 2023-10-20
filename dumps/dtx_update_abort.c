// -f VOS510 -X
// vts_dtx_abort_visibility
vts_dtx_begin // a test-specific dtx_begin() altenative
        vts_init_dte
                /** Use unique API so new UUID is generated even on same thread */
                daos_dti_gen_unique
        vos_dtx_rsrvd_init
        vos_dtx_attach(dth, persistent=false, exist=false) /* Generate DTX entry for the given DTX, and attach it to the DTX handle. */
                /* dtx_is_valid_handle(dth) == true */
                /* dth->dth_ent == NULL; Pointer to the DTX entry in DRAM */
                /* exist == false */
                /* persist == false */
                rc = vos_dtx_alloc(dbd, dth);
                        /* struct vos_dtx_act_ent *dae; */
                        cont = vos_hdl2cont(dth->dth_coh); /* VOS container (DRAM) */
                        /* struct lru_array *vc_dtx_array; Array for active DTX records */
                        rc = lrua_allocx(cont->vc_dtx_array, &idx, dth->dth_epoch, &dae);
                        /* struct dtx_id dae_xid; The DTX identifier. */
                        d_iov_set(&kiov, &DAE_XID(dae), sizeof(DAE_XID(dae)));
                        d_iov_set(&riov, dae, sizeof(*dae));
                        /* Update the value of the provided key, or insert it as a new key if there is no match.*/
                        /* daos_handle_t vc_dtx_active_hdl; The handle for active DTX table (dbtree_create_inplace_ex) */
                        /** the B+ tree for active DTXs. (DRAM) */
                        dbtree_upsert(cont->vc_dtx_active_hdl, BTR_PROBE_EQ, DAOS_INTENT_UPDATE, key=&kiov, value=&riov, NULL);
                        dth->dth_ent = dae; /* Pointer to the DTX entry in DRAM. */
                /* rc == 0 */
                /* persistent == false */
                dth->dth_pinned = 1;
        *dthp = dth; 
io_test_obj_update
        /* Prepare IO sink buffers for the specified arrays of the given object.*/
        vos_update_begin(arg->ctx.tc_co_hdl, arg->oid, epoch, flags, dkey, 1, iod, iod_csums, 0, &ioh, dth);
                /* dtx_is_real_handle(dth) == true */
                epoch = dth->dth_epoch;
                vos_check_akeys(iod_nr, iods); /* check iods[i].iod_name; akey for this iod */
                /* create a VOS I/O context */
                vos_ioc_create(coh, oid, read_only=false, epoch, iod_nr, iods, iods_csums, flags, NULL, dedup_th, dth, &ioc);
                        /* Initialize incarnation log information (just a DRAM-backed cache for the ilog search results?) */
                        vos_ilog_fetch_init(&ioc->ic_dkey_info); 
                        vos_ilog_fetch_init(&ioc->ic_akey_info);
                        vos_ioc_reserve_init(ioc, dth);
                                for (i = 0; i < ioc->ic_iod_nr; i++) {
                                        daos_iod_t *iod = &ioc->ic_iods[i];
                                        total_acts += iod->iod_nr;
                                /* struct umem_rsrvd_act *ic_rsrvd_scm; reserved SCM extents */
                                /* Allocate array of structures for reserved actions */
                                umem_rsrvd_act_alloc(vos_ioc2umm(ioc), &ioc->ic_rsrvd_scm, total_acts);
                                        D_ALLOC(buf, size);
                        /* dtx_is_valid_handle(dth) */
                        /* Allocate a timestamp set */
                        vos_ts_set_allocate(ts_set = &ioc->ic_ts_set, vos_flags, cflags, iod_nr, dth, cont->vc_pool->vp_sysdb);
                                /* dtx_is_valid_handle(dth) */
                                /* dth->dth_local == false */
                                tx_id = &dth->dth_xid;
                                size = VOS_TS_TYPE_AKEY + akey_nr;
                                array_size = size * sizeof((*ts_set)->ts_entries[0]);
                                D_ALLOC(*ts_set, sizeof(**ts_set) + array_size);
                                /* tx_id != NULL */
                                uuid_copy((*ts_set)->ts_tx_id.dti_uuid, tx_id->dti_uuid);
                                (*ts_set)->ts_tx_id.dti_hlc = tx_id->dti_hlc;
                                vos_ts_set_append_cflags(*ts_set, cflags);
                                        /* vos_ts_in_tx(ts_set) */
                        /* rc == 0 */
                        bioc = vos_data_ioctxt(vp = cont->vc_pool);
                                struct bio_meta_context *mc = vos_pool2mc(vp);
                                /* mc == NULL */
                                /* Use dummy I/O context when data blob doesn't exist */
                                return vp->vp_dummy_ioctxt;
                        ioc->ic_biod = bio_iod_alloc(bioc, vos_ioc2umm(ioc), sgl_cnt = iod_nr, read_only ? BIO_IOD_TYPE_FETCH : BIO_IOD_TYPE_UPDATE);
                                D_ALLOC(biod, offsetof(struct bio_desc, bd_sgls[sgl_cnt]));
                                return biod;
                        /* ioc->ic_biod != NULL */
                        dcs_csum_info_list_init(list = &ioc->ic_csum_list, nr = iod_nr);
                                daos_size_t initial_size = (sizeof(struct dcs_csum_info) + 8) * nr;
                                list->dcl_buf_size = initial_size;
                                memset(list, 0, sizeof(*list));
                                D_ALLOC(list->dcl_csum_infos, list->dcl_buf_size);
                        for (i = 0; i < iod_nr; i++) {
                                int iov_nr = iods[i].iod_nr;
                                bsgl = bio_iod_sgl(biod = ioc->ic_biod, i);
                                        return &biod->bd_sgls[idx];
                                bio_sgl_init(sgl = bsgl, nr = iov_nr);
                                        sgl->... = ...;
                                        D_ALLOC_ARRAY(sgl->bs_iovs, nr);
                        *ioc_pp = ioc;
                vos_space_hold(vos_cont2pool(ioc->ic_cont), flags, dkey, iod_nr, iods, iods_csums, &ioc->ic_space_held[0]);
                        vos_space_query(pool, &vps, false);
                                umempobj_get_heapusage(pool->vp_umm.umm_pool, &scm_used);
                                        switch (ph_p->up_store.store_type) {
                                        case DAOS_MD_PMEM:
                                                pmemobj_ctl_get(pop, "stats.heap.curr_allocated", curr_allocated);
                                /* SCM_TOTAL(vps) >= scm_used */
                                SCM_FREE(vps) = SCM_TOTAL(vps) - scm_used;
                                /* NVMe isn't configured for this VOS pool */
                                /* pool->vp_vea_info == NULL */
                        estimate_space(pool, dkey, iod_nr, iods, iods_csums, &space_est[0]);
                                scm += estimate_space_key(umm, dkey); /* DKey */
                                        size = vos_krec_size(&rbund);
                                        /* Add ample space assuming one tree node is added.  We could refine this later */
                                        size += 1024
                                for (i = 0; i < iod_nr; i++) {
                                        scm += estimate_space_key(umm, &iod->iod_name); /* AKey */
                                        csums = vos_csum_at(iods_csums, i);
                                        /* iod->iod_type == DAOS_IOD_SINGLE */
                                        media = vos_policy_media_select(pool, iod->iod_type, size, VOS_IOS_GENERIC);
                                                policy_io_size /* vos_policies[pool->vp_policy_desc.policy](pool, type, size); */
                                        /* media == DAOS_MEDIA_SCM */
                                        scm += vos_recx2irec_size(size, csums);
                                        /* Assume one more SV tree node created */
                                        scm += 256;
                        /* scm_left >= SCM_SYS(&vps) */
                        /* scm_left >= POOL_SCM_HELD(pool) */
                        /* scm_left >= space_est[DAOS_MEDIA_SCM] */ 
                        /* pool->vp_vea_info == NULL */
                /* rc == 0 */
                rc = dkey_update_begin(ioc);
                        for (i = 0; i < ioc->ic_iod_nr; i++) {
                                iod_set_cursor(ioc, i);
                                rc = akey_update_begin(ioc);
                                        for (i = 0; i < iod->iod_nr; i++) {
                                                /* iod->iod_type == DAOS_IOD_SINGLE */
                                                rc = vos_reserve_single(ioc, media, size);
                                                        reserve_space(ioc, DAOS_MEDIA_SCM, scm_size, &off);
                                                                /* media == DAOS_MEDIA_SCM */
                                                                vos_reserve_scm(ioc->ic_cont, ioc->ic_rsrvd_scm, size);
                                                                        umem_reserve(vos_cont2umm(cont), rsrvd_scm, size);
                                                                                /* umm->umm_ops->mo_reserve != NULL */
                                                                                pmem_reserve /* umm->umm_ops->mo_reserve(umm, act, size, UMEM_TYPE_ANY);*/
                                                                                        pmemobj_reserve(pop, (struct pobj_action *)act, size, type_num)
                                                                /* !UMOFF_IS_NULL(umoff) */
                                                        /* rc != NULL */
                                                        vos_irec_init_csum(irec, value_csum);
                                                        memset(&biov, 0, sizeof(biov));
                                                        /* media == DAOS_MEDIA_SCM */
                                                        bio_addr_set(&biov.bi_addr, media, off);
                                                /* rc == 0 */
                                /* rc == 0 */
                /* rc == 0 */
                *ioh = vos_ioc2ioh(ioc);
        bio_iod_prep(vos_ioh2desc(ioh), BIO_CHK_TYPE_IO, NULL, 0);
                iod_prep_internal(biod, type, bulk_ctxt, bulk_perm);
                        /* biod->bd_buffer_prep == NULL */
                        iod_map_iovs(biod, arg);
                                /* NVMe context IS allocated */
                                /* biod->bd_ctxt->bic_xs_ctxt != NULL */
                                bdb = iod_dma_buf(biod);
                                iod_fifo_in(biod, bdb);
                                iterate_biov(biod, arg ? bulk_map_one : dma_map_one, arg);
                                        for (i = 0; i < biod->bd_sgl_cnt; i++) {
                                                /* data == NULL */
                                                /* bsgl->bs_nr_out == 1 */
                                                for (j = 0; j < bsgl->bs_nr_out; j++) {
                                                        dma_map_one /* cb_fn(biod, biov, data); */
                                                                /* direct_scm_access(biod, biov) ==  true */
                                                                bio_iov_set_raw_buf(biov, umem_off2ptr(umem, bio_iov2raw_off(biov)));
                                iod_fifo_out(biod, bdb);
                        /* rc == 0 */
        /* rc == 0 */
        bsgl = vos_iod_sgl_at(ioh, 0);
                bio_iod_sgl(ioc->ic_biod, idx);
        bio_iod_copy(vos_ioh2desc(ioh), sgl, 1);
                iterate_biov(biod, copy_one, &arg);
                        for (i = 0; i < biod->bd_sgl_cnt; i++) {
                                /* data != NULL */
                                /* cb_fn == copy_one */
                                /* bsgl->bs_nr_out == 1 */
                                for (j = 0; j < bsgl->bs_nr_out; j++) {
                                        copy_one /* cb_fn(biod, biov, data); */
                                                while (arg->ca_iov_idx < sgl->sg_nr) {
                                                        /* buf_len > arg->ca_iov_off */
                                                        /* iov->iov_buf != NULL */
                                                        nob = min(size, buf_len - arg->ca_iov_off); /* 64 */
                                                        /* addr != NULL */
                                                        bio_memcpy(biod, media, addr, iov->iov_buf + arg->ca_iov_off, nob);
                                                                /* biod->bd_type == BIO_IOD_TYPE_UPDATE && media == DAOS_MEDIA_SCM */
                                                                /* !(DAOS_ON_VALGRIND && umem_tx_inprogress(umem)) */
                                                                umem_atomic_copy(umem, media_addr, addr, n, UMEM_RESERVED_MEM);
                                                                        pmem_atomic_copy /* umm->umm_ops->mo_atomic_copy(umm, dest, src, len, hint); */
                                                                                pmemobj_memcpy_persist(pop, dest, src, len);
                                                        /* biod->bd_type != BIO_IOD_TYPE_FETCH */
                                                        /* consumed an iov, move to the next */
                                                        /* arg->ca_iov_off == iov->iov_len */
        bio_iod_post(vos_ioh2desc(ioh), rc);
                /* No more actions for direct accessed SCM IOVs */
                /* biod->bd_rsrvd.brd_rg_cnt == 0 */
                iod_release_buffer(biod);
                        /* Release bulk handles */
                        bulk_iod_release(biod);
                                /* biod->bd_bulk_hdls == NULL */
                        /* No reserved DMA regions */
                        /* rsrvd_dma->brd_rg_cnt == 0 */
                /* !biod->bd_dma_issued && biod->bd_type == BIO_IOD_TYPE_UPDATE */
                iod_dma_completion(biod, biod->bd_result);
        /* rc == 0 && (arg->ta_flags & TF_ZERO_COPY) */
        vos_update_end(ioh, 0, dkey, rc, NULL, dth);
                vos_dedup_verify_fini(ioh);
                        /* ioc->ic_dedup_bsgls == NULL */
                vos_ts_set_add(ioc->ic_ts_set, ioc->ic_cont->vc_ts_idx, NULL, 0);
                        /* vos_ts_in_tx(ts_set) */
                        /* idx != NULL */
                        vos_ts_lookup(ts_set, idx, false, &entry)
                                vos_ts_lookup_internal(ts_set, type, idx, entryp);
                                        lrua_lookup(info->ti_array, idx, &entry);
                                                lrua_lookupx_(array, *idx, (uint64_t)idx, entryp);
                                                        entry = lrua_lookup_idx(array, idx, key, true);
                                                        /* entry == NULL */
                /* ts_set->ts_etype <= VOS_TS_TYPE_CONT */
                /* idx != NULL */
                entry = vos_ts_alloc(ts_set, idx, hash);
                        /* vos_ts_in_tx(ts_set) */
                        ts_table = vos_ts_table_get(false);
                        vos_ts_set_get_info(ts_table, ts_set, &info, &hash_offset);
                                /* ts_set->ts_init_count == 0 */
                                *info = &ts_table->tt_type_info[0];
                        hash_idx = vos_ts_get_hash_idx(info, hash, hash_offset);
                        vos_ts_evict_lru(ts_table, &new_entry, idx, hash_idx, info->ti_type);
                                lrua_alloc(ts_table->tt_type_info[type].ti_array, idx, &entry);
                                        lrua_allocx_(array, idx, (uint64_t)idx, entryp);
                                                lrua_find_free(array, &new_entry, idx, key);
                                                        sub_find_free(array, sub, entryp, idx, key)
                                                                /** Remove from free list */
                                                                lrua_remove_entry(array, sub, &sub->ls_free, entry, tree_idx);
                                                                /** Insert at tail (mru) */
                                                                lrua_insert(sub, &sub->ls_lru, entry, tree_idx, true);
                                /* neg_entry == NULL */
                                /** Use global timestamps for the type to initialize it */
                                vos_ts_copy(&entry->te_ts.tp_ts_rl, &entry->te_ts.tp_tx_rl, ts_table->tt_ts_rl, &ts_table->tt_tx_rl);
                                        daos_dti_copy(dest_id, src_id);
                                vos_ts_copy(&entry->te_ts.tp_ts_rh, &entry->te_ts.tp_tx_rh, ts_table->tt_ts_rh, &ts_table->tt_tx_rh);
                                        daos_dti_copy(dest_id, src_id);
                /* err == 0 */
                vos_tx_begin(dth, umem, ioc->ic_cont->vc_pool->vp_sysdb);
                        umem_tx_begin(umm, vos_txd_get(is_sysdb));
                                pmem_tx_begin /* umm->umm_ops->mo_tx_begin(umm, txd); */
                                        /* txd != NULL */
                                        pmemobj_tx_begin(pop, NULL, TX_PARAM_CB, pmem_stage_callback, txd, TX_PARAM_NONE);
                        /* rc == 0 */
                        dth->dth_local_tx_started = 1; /* !!! */
                /* dth->dth_dti_cos_count == 0 */
                /* struct vos_container        *ic_cont; */
                /* daos_unit_oid_t ic_oid; */
                /* daos_epoch_range_t ic_epr; */
                /* daos_epoch_t ic_bound; The epoch bound including uncertainty */
                /* struct vos_object *ic_obj; reference on the object; A cached object (DRAM data structure). */
                /* struct vos_ts_set *ic_ts_set; */
                err = vos_obj_hold(occ = vos_obj_cache_current(ioc->ic_cont->vc_pool->vp_sysdb), cont = ioc->ic_cont, oid = ioc->ic_oid, &ioc->ic_epr, ioc->ic_bound, flags = VOS_OBJ_CREATE | VOS_OBJ_VISIBLE, intent = DAOS_INTENT_UPDATE, &ioc->ic_obj, ts_set = ioc->ic_ts_set);
                        vos_obj_cache_current(standalone)
                                return vos_obj_cache_get(standalone);
                                        /* struct vos_tls; VOS thread local storage structure */
                                        /* struct daos_lru_cache *vtl_ocache; In-memory object cache for the PMEM object table */
                                        return vos_tls_get(standalone)->vtl_ocache;
                        struct vos_object *obj; /* A cached object (DRAM data structure). */
                        struct daos_llink *lret;
                        /* struct obj_lru_key; Local type for VOS LRU key. VOS LRU key must consist of Object ID and container UUID */
                        struct obj_lru_key lkey;
                        /* cont->vc_pool->vp_dying == 0 */
                        /* create == true */
                        /* visible_only == true */
                        void *create_flag = cont;
                        /* create_flag points to vos_container ???*/
                        /* Try to hold cont=75395cba, obj=22518002431819840.7291629702848970753.0.0 layout 0 create=true epr=0-1398c09a65c40001 */
                        /* Create the key for obj cache */
                        lkey.olk_cont = cont;
                        lkey.olk_oid = oid;
                        /* Find a ref in the cache \a lcache and take its reference. if reference is not found add it. */
                        /* common DBUG src/common/lru.c:224 daos_lru_ref_hold() Inserting 0x555557a7a9a0 item into LRU Hash table */
                        rc = daos_lru_ref_hold(lcache = occ, key = &lkey, ksize = sizeof(lkey), create_args = create_flag, llink = &lret);
                        /* rc == 0 */
                        /* Object is in cache */
                        /* container_of(); given a pointer @ptr to the field @member embedded into type (usually struct) @type, return pointer to the embedding instance of @type. */
                        /* struct daos_llink obj_llink; llink for daos lru cache */
                        obj = container_of(ptr = lret, type = struct vos_object, member = obj_llink);
                        /* obj->obj_zombie == false */
                        /* intent == DAOS_INTENT_UPDATE */
                        /* obj->obj_df != NULL */
                        /* create == true */
                        /* struct vos_obj_df *obj_df; Persistent memory address of the object; VOS object, assume all objects are KV store... NB: PMEM data structure. */
                        /* struct ilog_df vo_ilog; Incarnation log for the object; Opaque root for incarnation log */
                        vos_ilog_ts_ignore(umem = vos_obj2umm(obj), ilog = &obj->obj_df->vo_ilog);
                                /* DAOS_ON_VALGRIND == false */
                                return;
                        /* Check if the timestamps associated with the ilog are in cache.  If so, add them to the set. */
                        tmprc = vos_ilog_ts_add(ts_set, ilog = &obj->obj_df->vo_ilog, record = &oid, rec_size = sizeof(oid));
                                /* vos_ts_in_tx(ts_set) == true */
                                /* ilog != NULL */
                                uint32_t *idx = ilog_ts_idx_get(ilog_df = ilog);
                                        /*
                                        struct ilog_root {
                                                union {
                                                        struct ilog_id                lr_id;
                                                        struct ilog_tree        lr_tree;
                                                };
                                                uint32_t                        lr_ts_idx;
                                                uint32_t                        lr_magic;
                                        };
                                        */
                                        /* No validity check as index is just a constant offset */
                                        struct ilog_root *root = (struct ilog_root *)ilog_df;
                                        return &root->lr_ts_idx;
                                vos_ts_set_add(ts_set, idx, record, rec_size);
                                        uint64_t hash = 0;
                                        /* vos_ts_in_tx(ts_set) == true */
                                        /* idx != NULL */
                                        /* ts_set->ts_etype == 1; type of next entry */
                                        /* ts_set->ts_set_size == 4; size of the set */
                                        vos_ts_lookup(ts_set, idx, reset = false, &entry);
                                                /* found == false */
                                                /* entry == NULL */
                                        /* ts_set->ts_etype == 1 */
                                        /* ts_set->ts_etype > VOS_TS_TYPE_CONT (0) */
                                        /* ts_set->ts_etype == 1 */
                                        /* sysdb pool should not come here */
                                        /* ts_set->ts_etype == VOS_TS_TYPE_OBJ (1) */
                                        /* daos_unit_oid_t; 192-bit object ID, it can identify a unique bottom level object. (a shard of upper level object). */
                                        daos_unit_oid_t *oid = (daos_unit_oid_t *)rec;
                                        /* daos_obj_id_t id_pub; Public section, high level object ID */
                                        /* uint64_t lo; least significant (low) bits of object ID */
                                        /* uint64_t hi; most significant (high) bits of object ID */
                                        hash = oid->id_pub.lo ^ oid->id_pub.hi;
                                        /* idx != NULL */
                                        /* Allocate a new entry in the set. */
                                        entry = vos_ts_alloc(ts_set, idx, hash);
                                                /* (as above )*/
                                        /* entry != NULL */
                                        /* uint32_t ti_type; Type identifier */
                                        /* uint32_t ti_type; Type identifier */
                                        expected_type = entry->te_info->ti_type;
                                        /* ts_set->ts_init_count == 2 */
                                        struct vos_ts_set_entry        *se = &ts_set->ts_entries[ts_set->ts_init_count - 1];
                                        se->se_etype = ts_set->ts_etype;
                                        /* se->se_etype == 1 */
                                        /* se->se_etype > ts_set->ts_max_type */
                                        ts_set->ts_max_type = se->se_etype;
                                        /* expected_type != VOS_TS_TYPE_AKEY */
                                        ts_set->ts_etype = expected_type + 1;
                                        se->se_entry = entry;
                                        se->se_create_idx = NULL;
                        /* tmprc == 0 */
                        /* obj->obj_discard == false */
                        /* flags == VOS_OBJ_CREATE | VOS_OBJ_VISIBLE */
                        /* intent == DAOS_INTENT_UPDATE */
                        /* ts_set != NULL */
                        /* ts_set->ts_flags == 0 */
                        /* Check the incarnation log if an update is needed and update it.  Refreshes the log into \p entries.*/
                        /* struct vos_ilog_info        obj_ilog_info; Cache of incarnation log */
                        rc = vos_ilog_update_(cont, ilog = &obj->obj_df->vo_ilog, epr, bound, parent = NULL, info = &obj->obj_ilog_info, cond_flag = cond_mask, ts_set);
                                dth = vos_dth_get(cont->vc_pool->vp_sysdb);
                                daos_epoch_range_t max_epr = *epr;
                                /* parent == NULL */
                                bool has_cond = cond == VOS_ILOG_COND_UPDATE || cond == VOS_ILOG_COND_INSERT;
                                /* has_cond == false */
                                /* Checking and updating incarnation log in range 0-1398cddef0840001 */
                                /* Do a fetch first.  The log may already exist */
                                /* Read (or refresh) the incarnation log into \p entries.  Internally, this will be a noop if the arguments are the same and nothing has changed since the last invocation. */
                                rc = vos_ilog_fetch(umm = vos_cont2umm(cont), coh = vos_cont2hdl(cont), intent = DAOS_INTENT_UPDATE, ilog, epoch = epr->epr_hi, bound, has_cond, punched = NULL, parent, info);
                                        daos_epoch_range_t epr;
                                        epr.epr_lo = 0; /** Low bound of the epoch range */
                                        epr.epr_hi = epoch; /** High bound of the epoch range */
                                        vos_ilog_fetch_internal(umm, coh, intent, ilog, &epr, bound, has_cond, punched, parent, info);
                                                struct ilog_desc_cbs cbs; /* Near term hack to hook things up with existing DTX */
                                                /* Initialize callbacks for vos incarnation log */
                                                vos_ilog_desc_cbs_init(&cbs, coh);
                                                        cbs->dc_log_status_cb = vos_ilog_status_get;
                                                        cbs->dc_log_status_args = (void *)(unsigned long)coh.cookie;
                                                        cbs->dc_is_same_tx_cb = vos_ilog_is_same_tx;
                                                        cbs->dc_is_same_tx_args = (void *)(unsigned long)coh.cookie;
                                                        cbs->dc_log_add_cb = vos_ilog_add;
                                                        cbs->dc_log_add_args = NULL;
                                                        cbs->dc_log_del_cb = vos_ilog_del;
                                                        cbs->dc_log_del_args = (void *)(unsigned long)coh.cookie;
                                                /* Fetch the entire incarnation log.  This function will refresh only when the underlying log or the intent has changed.  If the struct is shared between multiple ULT's fetch should be done after every yield. */
                                                /* struct ilog_entries; Structure for storing the full incarnation log for ilog_fetch. */
                                                ilog_fetch(umm, root_df = ilog, &cbs, intent, has_cond, entries = &info->ii_entries);
                                                        struct ilog_root *root = (struct ilog_root *)root_df;
                                                        ilog_fetch_cached(umm, root, cbs, intent, has_cond, entries);
                                                                struct ilog_priv *priv = ilog_ent2priv(entries);
                                                                        /* uint8_t ie_priv[ILOG_PRIV_SIZE]; Private log data */
                                                                        return (struct ilog_priv *)&entries->ie_priv[0];
                                                                /* struct ilog_context ip_lctx; Embedded context for current log root */
                                                                struct ilog_context *lctx = &priv->ip_lctx;
                                                                /* struct ilog_root *ic_root; Root pointer */
                                                                /* priv->ip_lctx.ic_root == root */
                                                                /* int32_t ip_log_version; Version of log from prior fetch */
                                                                /* priv->ip_log_version != ilog_mag2ver(root->lr_magic) */
                                                                lctx->ic_root = root; /* Root pointer */
                                                                lctx->ic_root_off = umem_ptr2off(umm, root); /* umem offset of root pointer */
                                                                lctx->ic_umm = umm; /** umem instance */
                                                                lctx->ic_cbs = *cbs; /** Cache the callbacks */
                                                                lctx->ic_ref = 0; /** ref count for iterator */
                                                                lctx->ic_in_txn = false; /** In pmdk transaction marker */
                                                                lctx->ic_ver_inc = false; /** version needs incrementing */
                                                                entries->ie_num_entries = 0; /** Number of entries in the log */
                                                                priv->ip_intent = intent; /** Intent for prior fetch */
                                                                priv->ip_log_version = ilog_mag2ver(lctx->ic_root->lr_magic); /** Version of log from prior fetch */
                                                                priv->ip_rc = 0; /** Cached return code for fetch operation */
                                                                return false;
                                                        struct ilog_context *lctx = &priv->ip_lctx;
                                                        ilog_empty(root);
                                                                /** The ilog is split into two parts.   If there is one entry, the ilog
                                                                 *  is embedded into the root df struct.   If not, a b+tree is used.
                                                                 *  The tree is used more like a set where only the key is used.
                                                                 */
                                                                /* (!root->lr_tree.it_embedded) == false */
                                                                /* root->lr_tree.it_root != UMOFF_NULL (0) */
                                                                return !root->lr_tree.it_embedded && root->lr_tree.it_root == UMOFF_NULL;
                                                        struct ilog_array_cache cache;
                                                        ilog_log2cache(lctx, &cache);
                                                                /* ilog_empty(lctx->ic_root) == false */
                                                                /* lctx->ic_root->lr_tree.it_embedded == true */
                                                                /* struct ilog_id lr_id; */
                                                                /*
                                                                struct ilog_id {
                                                                        // DTX of entry
                                                                        union {
                                                                                uint64_t        id_value;
                                                                                struct {
                                                                                        uint32_t         id_tx_id;
                                                                                        uint16_t         id_punch_minor_eph;
                                                                                        uint16_t         id_update_minor_eph;
                                                                                };
                                                                        };
                                                                        // timestamp of entry
                                                                        daos_epoch_t        id_epoch;
                                                                };
                                                                */
                                                                /* struct ilog_id *ac_entries; Pointer to entries */
                                                                cache->ac_entries = &lctx->ic_root->lr_id;
                                                                cache->ac_nr = 1; /** Number of entries */
                                                                cache->ac_array = NULL; /** Pointer to array, if applicable */
                                                        rc = prepare_entries(entries, &cache);
                                                                /* cache->ac_nr <= NUM_EMBEDDED */
                                                                /* struct ilog_id *ie_ids; Array of log entries */
                                                                entries->ie_ids = cache->ac_entries;
                                                        /* rc == 0 */
                                                        /* intent == DAOS_INTENT_UPDATE */
                                                        /* has_cond == false */
                                                        /* retry == false */
                                                        for (i = 0; i < cache.ac_nr; i++) {
                                                                struct ilog_id *id = &cache.ac_entries[i];
                                                                status = ilog_status_get(lctx, id, intent, retry);
                                                                        struct ilog_desc_cbs *cbs = &lctx->ic_cbs;
                                                                        /* cbs->dc_log_status_cb != NULL */
                                                                        vos_ilog_status_get /* cbs->dc_log_status_cb(lctx->ic_umm, tx_id = id->id_tx_id, epoch = id->id_epoch, intent, retry, args = cbs->dc_log_status_args); */
                                                                                daos_handle_t coh;
                                                                                coh.cookie = (unsigned long)args;
                                                                                vos_dtx_check_availability(coh, entry = tx_id, epoch, intent, type = DTX_RT_ILOG, retry);
                                                                                        struct vos_container *cont = vos_hdl2cont(coh);
                                                                                        dth = vos_dth_get(cont->vc_pool->vp_sysdb);
                                                                                        /* dth != NULL */
                                                                                        /* dth->dth_for_migration == false */
                                                                                        /* type == DTX_RT_ILOG */
                                                                                        /* intent != DAOS_INTENT_CHECK */
                                                                                        dtx_is_committed(tx_lid = entry, cont, epoch);
                                                                                                /* tx_lid == DTX_LID_COMMITTED (0) */ /** Used for marking an in-tree record committed */
                                                                                /* rc == ALB_AVAILABLE_CLEAN (1) */ /* available, no (or not care) pending modification */
                                                                        /* rc == ILOG_COMMITTED (1) */ /** Log entry is visible to caller */
                                                                /* status == ILOG_COMMITTED (1) */
                                                                /* struct ilog_info *ie_info; Parsed information about each ilog entry */
                                                                entries->ie_info[entries->ie_num_entries].ii_removed = 0; /** Used internally to indicate removal during aggregation */
                                                                entries->ie_info[entries->ie_num_entries++].ii_status = status; /** Status of ilog entry */
                                                        /* entries->ie_num_entries != 0 */
                                                        priv->ip_rc = rc; /** Cached return code for fetch operation */
                                                /* rc == 0 */
                                                info->ii_uncommitted = 0; /** Visible uncommitted epoch */
                                                info->ii_create = 0; /** If non-zero, earliest creation timestamp in current incarnation. */
                                                info->ii_full_scan = true; /** All data is contained within specified epoch range */
                                                /** If non-zero, subsequent committed punch.  Minor epoch not used for
                                                 *  subsequent punch as it does not need replay if it's intermediate
                                                 */
                                                info->ii_next_punch = 0;
                                                /** True if there is an uncertain update.  If a punch is uncertain,
                                                 *  it should always cause a failure in vos_ilog_fetch.  But update
                                                 *  conflict depends on the operation doing the check.
                                                 */
                                                info->ii_uncertain_create = 0;
                                                info->ii_empty = true; /** The entity has no valid log entries */
                                                /* struct vos_punch_record ii_prior_punch; If non-zero, prior committed punch */
                                                info->ii_prior_punch.pr_epc = 0; /** Major epoch of punch */
                                                info->ii_prior_punch.pr_minor_epc = 0; /** Minor epoch of punch */
                                                /* struct vos_punch_record ii_prior_any_punch; If non-zero, prior committed or uncommitted punch */
                                                info->ii_prior_any_punch.pr_epc = 0;
                                                info->ii_prior_any_punch.pr_minor_epc = 0;
                                                rc = vos_parse_ilog(info, epr, bound, &punch);
                                                        ilog_foreach_entry_reverse(&info->ii_entries, &entry) {
                                                                /* vos_ilog_punched(&entry, punch) == false */
                                                                /* vos_ilog_punch_covered(&entry, &info->ii_prior_any_punch) == false */
                                                                /* entry.ie_status != ILOG_UNCOMMITTED */
                                                                /** We we have a committed entry that exceeds uncommitted epoch, clear the uncommitted epoch. */
                                                                /* entry.ie_id.id_epoch > info->ii_uncommitted */
                                                                info->ii_uncommitted = 0;
                                                                /* ilog_has_punch(&entry) == false */
                                                                info->ii_create = entry.ie_id.id_epoch;
                                                        /* epr->epr_lo == 0 */
                                                        /* vos_epc_punched(info->ii_prior_punch.pr_epc, info->ii_prior_punch.pr_minor_epc, punch) == true */
                                                        info->ii_prior_punch = *punch; /* == NULL */*/
                                                        /* vos_epc_punched(info->ii_prior_any_punch.pr_epc, info->ii_prior_any_punch.pr_minor_epc, punch) == true */
                                                        info->ii_prior_any_punch = *punch; /* == NULL */

                                                /* rc == 0 */
                                rc = vos_ilog_update_check(info, &max_epr);
                                        /* No need to refetch the log.  The only field that is used by update
                                         * is prior_any_punch.   This field will not be changed by ilog_update
                                         * for the purpose of parsing the child log. */
                                /* rc == 0 */
                                /* cond == 0 */
                        /* rc == 0 */
                        /* obj->obj_df != NULL */
                        obj->obj_sync_epoch = obj->obj_df->vo_sync;
                        /* obj != &obj_local */
                /* err == 0 */
                err = dkey_update(ioc, pm_ver, dkey, (dtx_is_real_handle(dth) ? dth->dth_op_seq : VOS_SUB_OP_MAX));
                        rc = obj_tree_init(obj);
                                /* daos_handle_is_valid(obj->obj_toh) == true */
                        struct vos_object *obj = ioc->ic_obj;
                        /* Persisted VOS (d/a)key record, it is referenced by btr_record::rec_off of btree VOS_BTR_DKEY/VOS_BTR_AKEY. */
                        struct vos_krec_df *krec;
                        daos_handle_t ak_toh;
                        /*
                         * daos_handle_t obj_toh; dkey tree open handle of the object (volatile)
                         * VOS_BTR_DKEY - distribution key tree
                         * SUBTR_CREATE < may create the subtree
                         * DAOS_INTENT_UPDATE - write/insert
                         */
                         /* Load the subtree roots embedded in the parent tree record. */
                        rc = key_tree_prepare(obj, toh = obj->obj_toh, tclass = VOS_BTR_DKEY, key = dkey, flags = SUBTR_CREATE, intent = DAOS_INTENT_UPDATE, &krec, sub_toh = &ak_toh, ts_set = ioc->ic_ts_set);
                                /* Data structure which carries the value buffers, checksums and memory IDs to the multi-nested btree. */
                                struct vos_rec_bundle rbund;
                                d_iov_t riov;
                                created = false;
                                /** reset the saved hash */
                                vos_kh_clear(obj->obj_cont->vc_pool->vp_sysdb);
                                /* krecp != NULL */
                                *krecp = NULL;
                                /**
                                 * store a bundle of parameters into a iovec, which is going to be passed
                                 * into dbtree operations as a compound value (data buffer address, or ZC
                                 * buffer umoff, checksum etc).
                                 */
                                tree_rec_bundle2iov(&rbund, &riov);
                                /* NB: In order to avoid complexities of passing parameters to the
                                * multi-nested tree, tree operations are not nested, instead:
                                *
                                * - In the case of fetch, we load the subtree root stored in the
                                *   parent tree leaf.
                                * - In the case of update/insert, we call dbtree_update() which may
                                *   create the root for the subtree, or just return it if it's already
                                *   there.
                                */
                                /* Search the provided \a key and fetch its value (and key if the matched key
                                 * is different with the input key). This function can support advanced range
                                 * search operation based on \a opc.
                                 */
                                rc = dbtree_fetch(toh, BTR_PROBE_EQ, intent, key, NULL, &riov);
                                /* rc == 0 */
                                /* struct vos_krec_df *rb_krec; Returned durable address of the btree record */
                                krec = rbund.rb_krec;
                                /* struct ilog_df kr_ilog; Incarnation log for key */
                                ilog = &krec->kr_ilog;
                                /** fall through to cache re-cache entry */
                                /* ilog != NULL && (flags & SUBTR_CREATE) */
                                vos_ilog_ts_ignore(vos_obj2umm(obj), &krec->kr_ilog);
                                        /* !DAOS_ON_VALGRIND */
                                /* Check if the timestamps associated with the ilog are in cache.  If so, add them to the set. */
                                /* void *iov_buf; buffer address */
                                /* size_t iov_len; data length */
                                tmprc = vos_ilog_ts_add(ts_set, ilog, record = key->iov_buf, rec_size = (int)key->iov_len);
                                        /* vos_ts_in_tx(ts_set) == true */
                                        /* ilog != NULL */
                                        idx = ilog_ts_idx_get(root = ilog);
                                                return &root->lr_ts_idx;
                                        vos_ts_set_add(ts_set, idx, record, rec_size);
                                                hash = vos_hash_get(buf = rec, len = rec_size, false);
                                                        return d_hash_murmur64(buf, len, BTR_MUR_SEED);
                                                /* idx != NULL */
                                                /* Allocate a new entry in the set. */
                                                entry = vos_ts_alloc(ts_set, idx, hash);
                                                        struct vos_ts_set_entry  set_entry = {0};
                                                        struct vos_ts_entry *new_entry;
                                                        ts_table = vos_ts_table_get(false);
                                                                /* struct vos_ts_table *vtl_ts_table; Timestamp table for xstream */
                                                                return vos_tls_get(standalone)->vtl_ts_table;
                                                        /* Use the parent entry to get the type info and hash offset for the current object/key. */
                                                        vos_ts_set_get_info(ts_table, ts_set, &info, &hash_offset);
                                                        /** By combining the parent entry offset, we avoid using the same
                                                         *  index for every key with the same value.
                                                         */
                                                        hash_idx = vos_ts_get_hash_idx(info, hash, hash_offset);
                                                        /* Internal function to evict LRU and initialize an entry */
                                                        /* uint32_t ti_type; Type identifier */
                                                        vos_ts_evict_lru(ts_table, &new_entry, idx, hash_idx, info->ti_type);
                                                        /* struct vos_ts_entry *se_entry; Pointer to the entry at this level */
                                                        set_entry.se_entry = new_entry;
                                                        /** No need to save allocation hash for non-negative entry */
                                                        /* struct vos_ts_set_entry ts_entries[0]; timestamp entries */
                                                        /* uint32_t ts_init_count; Number of initialized entries */
                                                        ts_set->ts_entries[ts_set->ts_init_count++] = set_entry;
                                /* tmprc == 0 */
                                /* sub_toh != NULL */
                                /* created == false */
                                rc = tree_open_create(obj, tclass, flags, krec, created, sub_toh);
                                        struct umem_attr *uma = vos_obj2uma(obj);
                                        struct vos_pool *pool = vos_obj2pool(obj);
                                        /* struct vos_container *obj_cont; backref to container */
                                        daos_handle_t coh = vos_cont2hdl(obj->obj_cont);
                                        struct evt_desc_cbs cbs; /* Callbacks and parameters for evtree descriptor */
                                        vos_evt_desc_cbs_init(&cbs, pool, coh);
                                        /* !(flags & SUBTR_EVT) */
                                        /* Open a btree from the root address. */
                                        /* struct btr_root kr_btr; btree root under the key */
                                        dbtree_open_inplace_ex(root = &krec->kr_btr, uma, coh, priv = pool, sub_toh);
                                                struct btr_context *tcx;
                                                /* Create a btree context (in volatile memory). */
                                                /* XXX The comment above is misleading. Note use of PMDK below. */
                                                rc = btr_context_create(root_off = BTR_ROOT_NULL, root, tree_class = -1, tree_feats = -1, tree_order = -1, uma, coh, priv, &tcx);
                                                        /**
                                                         * Initialize a tree instance from a registered tree class.
                                                         */
                                                        rc = btr_class_init(root_off, root, tree_class, &tree_feats, uma, coh, priv, &tcx->tc_tins);
                                                                /* Instantiate a memory class \a umm by attributes in \a uma */
                                                                rc = umem_class_init(uma, &tins->ti_umm);
                                                                        /** Workout the necessary offsets and base address for the pool */
                                                                        set_offsets(umm);
                                                                                /* umm->umm_id == UMEM_CLASS_PMEM */
                                                                                root_oid = pmemobj_root(pop, 0);
                                                                                root = pmemobj_direct(root_oid);
                                                                                umm->umm_pool_uuid_lo = root_oid.pool_uuid_lo;
                                                                                umm->umm_base = (uint64_t)root - root_oid.off;
                                                                /* tc->tc_feats & BTR_FEAT_DYNAMIC_ROOT */
                                                                *tree_feats |= BTR_FEAT_DYNAMIC_ROOT;
                                                        /* rc == 0 */
                                                        btr_context_set_depth(tcx, depth);
                                                        /** create handle for the tree context */
                                                        *toh = btr_tcx2hdl(tcx);
                                                /* rc == 0 */
                                /* rc == 0 */
                                /* For updates, we need to be able to modify the epoch range */
                                /* krecp != NULL */
                                *krecp = krec;
                        /* rc == 0 */
                        /* ioc->ic_ts_set != NULL */
                        rc = vos_ilog_update(ioc->ic_cont, &krec->kr_ilog, &ioc->ic_epr, ioc->ic_bound, &obj->obj_ilog_info, &ioc->ic_dkey_info, update_cond, ioc->ic_ts_set);
                                /* parent != NULL */
                                /** Do a fetch first.  The log may already exist */
                                rc = vos_ilog_fetch(vos_cont2umm(cont), vos_cont2hdl(cont), DAOS_INTENT_UPDATE, ilog, epr->epr_hi, bound, has_cond, NULL, parent, info);
                                        /* XXX */
                                /* rc == 0 */
                                rc = vos_ilog_update_check(info, &max_epr);
                                /* rc == 0 */
                        for (i = 0; i < ioc->ic_iod_nr; i++) {
                                iod_set_cursor(ioc, i);
                                rc = akey_update(ioc, pm_ver, ak_toh, minor_epc);
                                        rc = key_tree_prepare(obj, ak_toh, VOS_BTR_AKEY, &iod->iod_name, flags, DAOS_INTENT_UPDATE, &krec, &toh, ioc->ic_ts_set);
                                                rc = dbtree_fetch(toh, BTR_PROBE_EQ, intent, key, NULL, &riov);
                                                        rc = btr_probe_key(tcx, opc, intent, key);
                                                        /* rc == PROBE_RC_OK */
                                                        btr_rec_fetch(tcx, rec, key_out, val_out);
                                                /* XXX */
                                        /* rc == 1 */
                                        /* ioc->ic_ts_set != NULL */
                                        rc = vos_ilog_update(ioc->ic_cont, &krec->kr_ilog, &ioc->ic_epr, ioc->ic_bound, &ioc->ic_dkey_info, &ioc->ic_akey_info, update_cond, ioc->ic_ts_set);
                                                /* parent != NULL */
                                                /** Do a fetch first.  The log may already exist */
                                                rc = vos_ilog_fetch(vos_cont2umm(cont), vos_cont2hdl(cont), DAOS_INTENT_UPDATE, ilog, epr->epr_hi, bound, has_cond, NULL, parent, info);
                                                /* rc == 0 */
                                        /* rc == 0 */
                                        /* iod->iod_type == DAOS_IOD_SINGLE */
                                        rc = akey_update_single(toh, pm_ver, iod->iod_size, gsize, ioc, minor_epc);
                                                biov = iod_update_biov(ioc);
                                                        bsgl = bio_iod_sgl(ioc->ic_biod, ioc->ic_sgl_at);
                                                rc = dbtree_update(toh, &kiov, &riov);
                                                        rc = btr_tx_begin(tcx);
                                                                /* btr_has_tx(tcx) */
                                                                umem_tx_begin /* umem_tx_begin(btr_umm(tcx), NULL); */
                                                                        pmem_tx_begin /* umm->umm_ops->mo_tx_begin(umm, txd); */
                                                                                /* txd == NULL */
                                                                                pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
                                                        rc = btr_upsert(tcx, BTR_PROBE_EQ, DAOS_INTENT_UPDATE, key, val, NULL);
                                                                /* probe_opc != BTR_PROBE_BYPASS */
                                                                rc = btr_probe_key(tcx, probe_opc, intent, key);
                                                                        /* XXX */
                                                                /* rc == 1 */
                                                                switch (rc) {
                                                                case PROBE_RC_NONE:
                                                                        rc = btr_insert(tcx, key, val, val_out);
                                                                                /* tcx->tc_depth != 0 */
                                                                                rc = btr_node_insert_rec(tcx, trace, rec);
                                                                                        /* btr_root_resize_needed(tcx) == true */
                                                                                        rc = btr_root_resize(tcx, trace, &node_alloc);
                                                                                                /* btr_has_tx(tcx) == true */
                                                                                                rc = btr_root_tx_add(tcx);
                                                                                                        /* !UMOFF_IS_NULL(tins->ti_root_off) */
                                                                                                        rc = umem_tx_add_ptr(btr_umm(tcx), tcx->tc_tins.ti_root, sizeof(struct btr_root));
                                                                                                                pmem_tx_add_ptr /* umm->umm_ops->mo_tx_add_ptr(umm, ptr, size); */
                                                                                                                        rc = pmemobj_tx_add_range_direct(ptr, size);
                                                                                                rc = btr_node_alloc(tcx, &nd_off);
                                                                                                        /* btr_ops(tcx)->to_node_alloc != NULL */
                                                                                                        svt_node_alloc /* nd_off = btr_ops(tcx)->to_node_alloc(&tcx->tc_tins, btr_node_size(tcx)); */ 
                                                                                                                pmem_tx_alloc /* umem_zalloc(&tins->ti_umm, size); */
                                                                                                                        pmemobj_tx_xalloc(size, type_num, pflags)
                                                                                                /* rc == 0 */
                                                                                                memcpy(btr_off2ptr(tcx, nd_off), nd, old_size);
                                                                                                btr_node_free(tcx, old_node);
                                                                                                        pmem_tx_free /* rc = umem_free(btr_umm(tcx), nd_off); */
                                                                                                        /* !UMOFF_IS_NULL(umoff) */
                                                                                                        rc = pmemobj_tx_free(umem_off2id(umm, umoff));
                                                                                        /* rc == 0 */
                                                                                        /* !btr_node_is_full(tcx, trace->tr_node) */
                                                                                         rc = btr_node_insert_rec_only(tcx, trace, rec);
                                                                                                /* nd->tn_keyn > 0 */
                                                                                                rc = btr_check_availability(tcx, &alb);
                                                                                                        /* btr_ops(tcx)->to_check_availability != NULL */
                                                                                                        /* btr_node_is_leaf(tcx, alb->nd_off) == true */
                                                                                                        rec = btr_node_rec_at(tcx, alb->nd_off, alb->at);
                                                                                                        svt_check_availability /* rc = btr_ops(tcx)->to_check_availability(&tcx->tc_tins, rec, alb->intent); */
                                                                                                                vos_dtx_check_availability(tins->ti_coh, svt->ir_dtx, *epc, intent, DTX_RT_SVT, true);
                                                                                                                        /* type == DTX_RT_SVT */
                                                                                                                        /* intent == DAOS_INTENT_CHECK (5) */
                                                                                                                        /* XXX */
                                                                                                        /* rc == ALB_AVAILABLE_CLEAN (1) */
                                                                                                /* rc == PROBE_RC_OK (2) */
                                                                                                rec_a = btr_node_rec_at(tcx, trace->tr_node, trace->tr_at);
                                                                                                /* reuse == false */
                                                                                                /* trace->tr_at == nd->tn_keyn */
                                                                                                btr_rec_copy(tcx, rec_a, rec, 1);
                                                                                                        memcpy(dst_rec, src_rec, rec_nr * btr_rec_size(tcx));
                                                                                        /* rc == 0 */
                                                                                /* rc == 0 */
                                                                tcx->tc_probe_rc = PROBE_RC_UNKNOWN; /* path changed */
                                                        btr_tx_end(tcx, rc);
                                                                /* btr_has_tx(tcx) == true */
                                                                rc = umem_tx_commit(btr_umm(tcx));
                                                                        umem_tx_commit_ex(umm, NULL);
                                                                                pmem_tx_commit /* umm->umm_ops->mo_tx_commit(umm, data); */
                                                                                        pmemobj_tx_commit();
                                                                                        rc = pmemobj_tx_end();
                                        /* rc == 0 */
                                        /* daos_handle_is_valid(toh) */
                                        key_tree_release(toh, is_array);
                                                /* is_array == false */
                                                rc = dbtree_close(toh);
                                                        btr_context_decref(tcx);
                                                                rc = vos_key_mark_agg(ioc->ic_cont, krec, ioc->ic_epr.epr_hi);
                                                                        /* XXX */
                        key_tree_release(ak_toh, false);
                                /* is_array == false */
                                rc = dbtree_close(toh);
                                        btr_context_decref(tcx);
vts_dtx_end // a test-specific dtx_end() altenative?
        /* dth->dth_shares_inited == true */
        dth->dth_share_tbd_count = 0;
        vos_dtx_rsrvd_fini(dth);
                /* dth->dth_rsrvds != NULL */
                vos_dtx_detach(dth);
                        dae = dth->dth_ent;
                        /* dae != NULL */
vos_dtx_abort
        rc = vos_dtx_abort(args->ctx.tc_co_hdl, &xid, epoch);
                rc = dbtree_lookup(cont->vc_dtx_active_hdl, &kiov, &riov);
                        dbtree_fetch(toh, BTR_PROBE_EQ, DAOS_INTENT_DEFAULT, key, NULL, val_out);
                                rc = btr_verify_key(tcx, key);
                                        rc = btr_probe_key(tcx, opc, intent, key);
                                                /* XXX */
                                        /* rc == PROBE_RC_OK (2) */
                                        rec = btr_trace2rec(tcx, tcx->tc_depth - 1);
                                                btr_node_rec_at(tcx, trace->tr_node, trace->tr_at);
                                        btr_rec_fetch(tcx, rec, key_out, val_out);
                                                dtx_act_ent_fetch /* btr_ops(tcx)->to_rec_fetch(&tcx->tc_tins, rec, key, val); */
                /* rc == 0 */
                /* !vos_dae_is_commit(dae) */
                /* !vos_dae_is_abort(dae) */
                /* !unlikely(dae->dae_preparing) */
                rc = vos_dtx_abort_internal(cont, dae, false);
                        rc = umem_tx_begin(umm, NULL);
                                pmem_tx_begin /* umm->umm_ops->mo_tx_begin(umm, txd); */
                                        /* txd == NULL */
                                        rc = pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
                        rc = dtx_rec_release(cont, dae, true);
                                /* umoff_is_null(dae_df->dae_mbs_off) == true */
                                /* dae->dae_records == NULL */
                                count = DAE_REC_CNT(dae);
                                for (i = count - 1; i >= 0; i--) {
                                        rc = do_dtx_rec_release(umm, cont, dae, DAE_REC_INLINE(dae)[i], abort);
                                                /* dtx_umoff_flag2type(rec) == DTX_RT_SVT */
                                                /* abort == true */
                                                /* DAE_INDEX(dae) != DTX_INDEX_INVAL */
                                                rc = umem_tx_add_ptr(umm, &svt->ir_dtx, sizeof(svt->ir_dtx));
                                                        pmem_tx_add_ptr /* umm->umm_ops->mo_tx_add_ptr(umm, ptr, size); */
                                                                rc = pmemobj_tx_add_range_direct(ptr, size);
                                                dtx_set_aborted(&svt->ir_dtx);
                                        /* rc  == 0 */
                                /* dbd->dbd_index < dbd->dbd_cap */
                                rc = umem_tx_add_ptr(umm, &dae_df->dae_flags, sizeof(dae_df->dae_flags));
                                        pmem_tx_add_ptr /* umm->umm_ops->mo_tx_add_ptr(umm, ptr, size); */
                                                rc = pmemobj_tx_add_range_direct(ptr, size);
                                /* Mark the DTX entry as invalid in SCM. */
                                dae_df->dae_flags = DTE_INVALID;
                                rc = umem_tx_add_ptr(umm, &dbd->dbd_count, sizeof(dbd->dbd_count));
                                        pmem_tx_add_ptr /* umm->umm_ops->mo_tx_add_ptr(umm, ptr, size); */
                                                rc = pmemobj_tx_add_range_direct(ptr, size);
                                dbd->dbd_count--;
                        dae->dae_preparing = 0;
                        /* rc == 0 */
                        dae->dae_aborting = 1;
                        rc = umem_tx_commit(umm);
                                umem_tx_commit_ex(umm, NULL);
                                        pmem_tx_commit /* umm->umm_ops->mo_tx_commit(umm, data); */
                                                pmemobj_tx_commit();
                                                rc = pmemobj_tx_end();
                        /* rc == 0 */
                        vos_dtx_post_handle(cont, &dae, NULL, 1, abort = true, rollback = false);
                                for (i = 0; i < count; i++) {
                                        rc = dbtree_delete(cont->vc_dtx_active_hdl, BTR_PROBE_EQ, &kiov, NULL);
                                                rc = btr_probe_key(tcx, opc, DAOS_INTENT_PUNCH, key);
                                                        /* XXX */
                                                /* rc == PROBE_RC_OK (2) */
                                                rc = btr_tx_delete(tcx, args);
                                                        rc = btr_tx_begin(tcx);
                                                                /* ! btr_has_tx(tcx) */
                                                        rc = btr_delete(tcx, args);
                                                                for (cur_tr = &tcx->tc_trace[tcx->tc_depth - 1];; cur_tr = par_tr) {
                                                                        /* root */
                                                                        /* cur_tr == tcx->tc_trace */
                                                                        rc = btr_root_del_rec(tcx, cur_tr, args);
                                                                                /* btr_node_is_leaf(tcx, trace->tr_node) == true */
                                                                                /* the root is NOT a leaf node */
                                                                                /* node->tn_keyn <= 1 */
                                                                                rc = btr_node_destroy(tcx, trace->tr_node, args, NULL);
                                                                                        /* leaf == true */
                                                                                        for (i = nd->tn_keyn - 1; i >= 0; i--) {
                                                                                                rec = btr_node_rec_at(tcx, nd_off, i);
                                                                                                        /* XXX */
                                                                                                rc = btr_rec_free(tcx, rec, args);
                                                                                                        dtx_act_ent_free /* rc = btr_ops(tcx)->to_rec_free(&tcx->tc_tins, rec, args); */
                                                                                                                /* dae != NULL */
                                                                                                                d_list_del_init(&dae->dae_link);
                                                                                                                /* dae != NULL */
                                                                                                                dtx_act_ent_cleanup(tins->ti_priv, dae, NULL, true);
                                                                                                                        /* evict == true */
                                                                                                                        /* dth == NULL */
                                                                                                                        for (i = 0; i < count; i++)
                                                                                                                                vos_obj_evict_by_oid(vos_obj_cache_current(cont->vc_pool->vp_sysdb), cont, oids[i]);
                                                                                                                                        rc = daos_lru_ref_hold(occ, &lkey, sizeof(lkey), NULL, &lret);
                                                                                                                                        daos_lru_ref_evict(occ, lret);
                                                                                                                                                d_hash_rec_evict_at(&lcache->dlc_htable, &llink->ll_link);
                                                                                                                                        daos_lru_ref_release(occ, lret);
                                                                                                                                                /* llink->ll_evicted == true */
                                                                                                                                                lru_del_evicted(lcache, llink);
                                                                                                                                                        d_hash_rec_delete_at(&lcache->dlc_htable, &llink->ll_link);
                                                                                                                                                                lru_hop_rec_free /* htable->ht_ops->hop_rec_free(htable, link); */
                                                                                                                                                                        obj_lop_free /* llink->ll_ops->lop_free_ref(llink); */
                                                                                                                                                                                clean_object(obj);
                                                                                                                                                                                        vos_ilog_fetch_finish
                                                                                                                                                                                                ilog_fetch_finish(&info->ii_entries);
                                                                                                                                                                                        vos_cont_decref(obj->obj_cont);
                                                                                                                                                                                                cont_decref(cont);
                                                                                                                                                                                                        d_uhash_link_putref(vos_cont_hhash_get(cont->vc_pool->vp_sysdb), &cont->vc_uhlink);
                                                                                                                                                                                                                d_hash_rec_decref(htable, &ulink->ul_link.rl_link);
                                                                                                                                                                                        obj_tree_fini(obj);
                                                                                                                                                                                                rc = dbtree_close(obj->obj_toh);
                                                                                                                                                                                                        btr_context_decref(tcx);
                                                                                                                                                                                                                D_FREE(tcx);
                                                                                                                                                while (!d_list_empty(&lcache->dlc_lru)) {
                                                                                        /* empty == true */
                                                                                        rc = btr_node_free(tcx, nd_off);
                                                                                                rc = umem_free(btr_umm(tcx), nd_off);
                                                                                                        vmem_free
                                                                                                                free
                                                                                /* btr_has_tx(tcx) == false */
                                                                                btr_context_set_depth(tcx, 0);
                                                        btr_tx_end(tcx, rc);
                                                                /* btr_has_tx(tcx) == false */
                                                tcx->tc_probe_rc = PROBE_RC_UNKNOWN;
                                        /* rc == 0 */
                                        dtx_evict_lid(cont, daes[i]);
                                                lrua_evictx
                                                        evict_cb(array, sub, entry, ent_idx);
                                                                /* array->la_cbs.lru_on_evict == NULL */
                                                                /** By default, reset the entry */
                                                                memset(entry->le_payload, 0, array->la_payload_size);
                                                        /** Remove from active list */
                                                        lrua_remove_entry(array, sub, &sub->ls_lru, entry, ent_idx);
                                                        lrua_insert(sub, &sub->ls_free, entry, ent_idx, (array->la_flags & LRU_FLAG_REUSE_UNIQUE) != 0);
/* rc == 0 */
