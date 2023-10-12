// -f VOS510
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
                /* daos_size_t ic_space_held[DAOS_MEDIA_MAX]; */
                vos_space_hold(pool = vos_cont2pool(ioc->ic_cont), flags, dkey, iod_nr, iods, iods_csums, &ioc->ic_space_held[0]);
                        /* struct vos_pool_space vps; */
                        vos_space_query(pool, &vps, slow = false);
                                SCM_TOTAL(vps) = df->pd_scm_sz;
                                SCM_SYS(vps) = POOL_SCM_SYS(pool);
                                /* Obtain the usage statistics for the pmem object */
                                /* daos_size_t scm_used; */
                                umempobj_get_heapusage(ph_p = pool->vp_umm.umm_pool, curr_allocated = &scm_used);
                                        /* ph_p->up_store.store_type == DAOS_MD_PMEM */
                                        pmemobj_ctl_get(pop, "stats.heap.curr_allocated", curr_allocated);
                                /* SCM_TOTAL(vps) >= scm_used */
                                SCM_FREE(vps) = SCM_TOTAL(vps) - scm_used;
                                /* NVMe isn't configured for this VOS pool */
                                /* pool->vp_vea_info == NULL */
                                NVME_TOTAL(vps) = 0;
                                NVME_FREE(vps) = 0;
                                NVME_SYS(vps) = 0;
                        /*
                        * Estimate how much space will be consumed by an update request. This
                        * conservative estimation always assumes new object, dkey, akey will be
                        * created for the update.
                        */
                        /* daos_size_t space_est[DAOS_MEDIA_MAX] = { 0, 0 }; */
                        estimate_space(pool, dkey, iod_nr, iods, iods_csums, &space_est[0]);
                                /* struct umem_instance	*umm = vos_pool2umm(pool); */
                                scm += estimate_space_key(umm, dkey); /* DKey */
                                        size = vos_krec_size(&rbund);
                                        /* Add ample space assuming one tree node is added. We could refine this later */
                                        size += 1024
                                for (i = 0; i < iod_nr; i++) {
                                        scm += estimate_space_key(umm, &iod->iod_name); /* AKey */
                                        csums = vos_csum_at(iods_csums, i);
                                        /* iod->iod_type == DAOS_IOD_SINGLE */
                                        media = vos_policy_media_select(pool, iod->iod_type, size, VOS_IOS_GENERIC);
                                                policy_io_size /* vos_policies[pool->vp_policy_desc.policy](pool, type, size); */
                                        /* media == DAOS_MEDIA_SCM */
                                        scm += vos_recx2irec_size(size, csums);
                                        /* Assume one more SV (single value?) tree node created */
                                        scm += 256;
                        scm_left = SCM_FREE(&vps);
                        /* scm_left >= SCM_SYS(&vps) */
                        scm_left -= SCM_SYS(&vps);
                        /* scm_left >= POOL_SCM_HELD(pool) */
                        scm_left -= POOL_SCM_HELD(pool);
                        /* scm_left >= space_est[DAOS_MEDIA_SCM] */ 
                        /* pool->vp_vea_info == NULL; In-memory free space tracking for NVMe device */
                        /* Note: The same would be done for DAOS_MEDIA_NVME. */
                        space_hld[DAOS_MEDIA_SCM]	= space_est[DAOS_MEDIA_SCM];
                        /** Held space by in-flight updates. In bytes */
	                POOL_SCM_HELD(pool)		+= space_hld[DAOS_MEDIA_SCM];
                /* rc == 0 */
                rc = dkey_update_begin(ioc);
                        for (i = 0; i < ioc->ic_iod_nr; i++) {
                                iod_set_cursor(ioc, sgl_at = i);
                                        /** cursor of SGL & IOV in BIO descriptor */
                                        ioc->ic_sgl_at = sgl_at;
	                                ioc->ic_iov_at = 0;
                                rc = akey_update_begin(ioc);
                                        for (i = 0; i < iod->iod_nr; i++) {
                                                /* iod->iod_type == DAOS_IOD_SINGLE */
                                                size = iod->iod_size;
                                                media = vos_policy_media_select(vos_cont2pool(ioc->ic_cont), iod->iod_type, size, VOS_IOS_GENERIC);
                                                /* media == DAOS_MEDIA_SCM */
                                                /* Reserve single value record on specified media */
                                                rc = vos_reserve_single(ioc, media, size);
                                                        struct dcs_csum_info *value_csum = vos_csum_at(iod_csums = ioc->ic_iod_csums, ioc->ic_sgl_at);
                                                                /* iod_csums == NULL */
                                                        /* value_csum == NULL */
                                                        /* media == DAOS_MEDIA_SCM */
                                                        scm_size = vos_recx2irec_size(size, value_csum);
                                                        /* uint64_t off = 0; */
                                                        reserve_space(ioc, media = DAOS_MEDIA_SCM, size = scm_size, &off);
                                                                /* media == DAOS_MEDIA_SCM */
	                                                        /* struct umem_rsrvd_act *ic_rsrvd_scm; reserved SCM extents */
                                                                umem_off_t umoff = vos_reserve_scm(ioc->ic_cont, rsrvd_scm = ioc->ic_rsrvd_scm, size);
                                                                        umem_reserve(vos_cont2umm(cont), rsrvd_act = rsrvd_scm, size);
                                                                                act = rsrvd_act->rs_actv + act_size * rsrvd_act->rs_actv_at;
                                                                                /* umm->umm_ops->mo_reserve != NULL */
                                                                                pmem_reserve /* umm->umm_ops->mo_reserve(umm, act, size, UMEM_TYPE_ANY);*/
                                                                                        pmemobj_reserve(pop, (struct pobj_action *)act, size, type_num)
                                                                /* !UMOFF_IS_NULL(umoff) */
                                                                ioc->ic_umoffs[ioc->ic_umoffs_cnt] = umoff;
                                                                ioc->ic_umoffs_cnt++;
                                                                *off = umoff;
                                                        /* rc != NULL */
                                                        umoff = ioc->ic_umoffs[ioc->ic_umoffs_cnt - 1];
                                                        /* struct vos_irec_df *irec; Persisted VOS single value & epoch record, it is referenced by btr_record::rec_off of btree VOS_BTR_SINGV. */ 
                                                	irec = (struct vos_irec_df *)umem_off2ptr(vos_ioc2umm(ioc), umoff);
                                                        vos_irec_init_csum(irec, csum = value_csum);
                                                                /* csum == NULL */
                                                                irec->ir_cs_size = 0; /** key checksum size (in bytes) */
                                                                irec->ir_cs_type = 0; /** key checksum type */
                                                        /* struct bio_iov biov; */
                                                        memset(&biov, 0, sizeof(biov));
                                                        /* media == DAOS_MEDIA_SCM */
                                                        bio_addr_set(addr = &biov.bi_addr, type = media, off);
                                                        	addr->ba_type = type; /* DAOS_MEDIA_SCM or DAOS_MEDIA_NVME */
	                                                        addr->ba_off = umem_off2offset(off); /* Byte offset within PMDK pmemobj pool for SCM; */
                                                /* rc == 0 */
                                /* rc == 0 */
                /* rc == 0 */
                *ioh = vos_ioc2ioh(ioc);
        /* Prepare all the SG lists of an io descriptor. */
        /* struct vos_io_context { ... struct bio_desc *ic_biod;} BIO descriptor, has ic_iod_nr SGLs */
        /* BIO_CHK_TYPE_IO - For IO request; CHK stands for chunk as in bio_chunk_type */
        bio_iod_prep(biod = vos_ioh2desc(ioh), type = BIO_CHK_TYPE_IO, bulk_ctxt = NULL, bulk_perm = 0);
                iod_prep_internal(biod, type, bulk_ctxt, bulk_perm);
                        /* biod->bd_buffer_prep == 0; XXX */
                        void  *arg = NULL;
                        biod->bd_chk_type = type;
                        biod->bd_rdma = (bulk_ctxt != NULL) || (type == BIO_CHK_TYPE_REBUILD); /* 0 */
                        /* bulk_ctxt == NULL */
                        iod_map_iovs(biod, arg);
                                /* NVMe context IS allocated */
                                /* biod->bd_ctxt->bic_xs_ctxt != NULL */
                                bdb = iod_dma_buf(biod);
                                        /* struct bio_desc; I/O descriptor */
                                        /* struct bio_io_context *bd_ctxt; Per VOS instance I/O context */
                                        /* struct bio_xs_context *bic_xs_ctxt; Per-xstream NVMe context */
                                        /* struct bio_dma_buffer *bxc_dma_buf; Per-xstream DMA buffer, used as SPDK dma I/O buffer or as temporary RDMA buffer for ZC fetch/update over NVMe devices. */
                                        return biod->bd_ctxt->bic_xs_ctxt->bxc_dma_buf;
                                iod_fifo_in(biod, bdb);
                                        /* bdb != NULL */
                                        /* bdb->bdb_queued_iods == 0 */
                                        return; /* No prior waiters */
                                /* arg == NULL */
                                iterate_biov(biod, arg ? bulk_map_one : dma_map_one, arg);
                                        /* struct bio_desc {
                                                ...
                                                // SG lists involved in this io descriptor
                                                unsigned int bd_sgl_cnt; 
                                                struct bio_sglist bd_sgls[0];
                                        };
                                        */
                                        for (i = 0; i < biod->bd_sgl_cnt; i++) {
                                                struct bio_sglist *bsgl = &biod->bd_sgls[i];
                                                /* data == NULL */
                                                /* bsgl->bs_nr_out == 1 */
                                                /*
                                                struct bio_sglist {
                                                        struct bio_iov	*bs_iovs;
                                                        unsigned int	 bs_nr;
                                                        unsigned int	 bs_nr_out;
                                                };
                                                */
                                                for (j = 0; j < bsgl->bs_nr_out; j++) {
                                                        struct bio_iov *biov = &bsgl->bs_iovs[j];
                                                        /* Convert offset of @biov into memory pointer */
                                                        dma_map_one /* cb_fn(biod, biov, data); */
                                                                /* biov->bi_data_len == 64; Data length in bytes */
                                                                /* bio_addr_is_hole(&biov->bi_addr); The address is NOT a hole */
                                                                /* direct_scm_access(biod, biov) ==  true */
                                                                        /*
                                                                        * Direct access SCM when:
                                                                        *
                                                                        * - It's inline I/O, or;
                                                                        * - Direct SCM RDMA enabled, or;
                                                                        * - It's deduped SCM extent;
                                                                        */
                                                                umem = biod->bd_umem;
                                                                bio_iov_set_raw_buf(biov, umem_off2ptr(umem, bio_iov2raw_off(biov)));
                                                                        /* For SCM, it's direct memory address of 'ba_off'; */
                                                                        biov->bi_buf = val;
                                iod_fifo_out(biod, bdb);
                                        /* biod->bd_in_fifo == 0*/
                                        return;
                        /* rc == 0 */
                        /* struct bio_rsrvd_dma bd_rsrvd; DMA buffers reserved by this io descriptor */
                        /* unsigned int brd_rg_cnt; Total number of reserved regions */
                        /* All direct SCM access, no DMA buffer prepared */
                        /* biod->bd_rsrvd.brd_rg_cnt == 0 */
                        return 0;
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
                vos_obj_hold(vos_obj_cache_current(ioc->ic_cont->vc_pool->vp_sysdb), ioc->ic_cont, ioc->ic_oid, &ioc->ic_epr, ioc->ic_bound, VOS_OBJ_CREATE | VOS_OBJ_VISIBLE, DAOS_INTENT_UPDATE, &ioc->ic_obj, ioc->ic_ts_set);
                        /* create == true */
                        daos_lru_ref_hold(occ, &lkey, sizeof(lkey), create_flag, &lret);
                                link = d_hash_rec_find(&lcache->dlc_htable, key, key_size);
                                        idx = ch_key_hash(htable, key, ksize);
                                        ch_bucket_lock(htable, idx, !is_lru);
                                        link = ch_rec_find(htable, bucket, key, ksize, D_HASH_LRU_HEAD);
                                                ch_key_cmp(htable, link, key, ksize)
                                                        obj_lop_cmp_key /* llink->ll_ops->lop_cmp_keys(key, ksize, llink); */
                                        /* link != NULL */
                                        ch_rec_addref(htable, link);
                                                lru_hop_rec_addref /* htable->ht_ops->hop_rec_addref != NULL */
                                        ch_bucket_unlock(htable, idx, !is_lru);
                                /* link != NULL */
                                /* !d_list_empty(&llink->ll_qlink) */
                                d_list_del_init(&llink->ll_qlink);
                                *llink_pp = llink;
                        /* rc == 0 */
                        /** Object is in cache */
                        obj = container_of(lret, struct vos_object, obj_llink);
                        /* !obj->obj_zombie */
                        /* intent == DAOS_INTENT_UPDATE */
                        /* obj->obj_df != NULL */
                        /* create == true */
                        vos_ilog_ts_ignore(vos_obj2umm(obj), &obj->obj_df->vo_ilog);
                                /* XXX */
                        tmprc = vos_ilog_ts_add(ts_set, &obj->obj_df->vo_ilog, &oid, sizeof(oid));
                                /* vos_ts_in_tx(ts_set) == true */
                                /* ilog != NULL */
                                idx = ilog_ts_idx_get(ilog);
                                vos_ts_set_add(ts_set, idx, record, rec_size);
                                        /* vos_ts_in_tx(ts_set) == true */
                                        /* idx != NULL */
                                        vos_ts_lookup(ts_set, idx, false, &entry)
                                                vos_ts_lookup_internal(ts_set, type, idx, entryp);
                                                        found = lrua_lookup(info->ti_array, idx, &entry);
                                                                lrua_lookupx_(array, *idx, (uint64_t)idx, entryp);
                                                                        lrua_lookup_idx(array, idx, key, true);
                                                                                sub = lrua_idx2sub(array, idx);
                                                                        /* entry == NULL */
                                                        /* found == false */
                                        /* ts_set->ts_etype > VOS_TS_TYPE_CONT */
                                        /* ts_set->ts_etype == VOS_TS_TYPE_OBJ */
                                        /* idx != NULL */
                                        entry = vos_ts_alloc(ts_set, idx, hash);
                                                /* vos_ts_in_tx(ts_set) == true */
                                                vos_ts_set_get_info(ts_table, ts_set, &info, &hash_offset);
                                                        /* ts_set->ts_init_count != 0 */
                                                hash_idx = vos_ts_get_hash_idx(info, hash, hash_offset);
                                                vos_ts_evict_lru(ts_table, &new_entry, idx, hash_idx, info->ti_type);
                                                        lrua_alloc(ts_table->tt_type_info[type].ti_array, idx, &entry);
                                                                lrua_allocx_(array, idx, (uint64_t)idx, entryp);
                                                                        lrua_find_free(array, &new_entry, idx, key);
                                                                                sub_find_free(array, sub, entryp, idx, key)
                                                                                        lrua_remove_entry(array, sub, &sub->ls_free, entry, tree_idx);
                                                                                        lrua_insert(sub, &sub->ls_lru, entry, tree_idx, true);
                                                        /* rc == 0 */
                                                        /* neg_entry != NULL */
                                                        vos_ts_copy
                                                                /* XXX */
                                                        vos_ts_copy
                                                                /* XXX */
                                        /* entry != NULL */
                        /* tmprc == 0 */
                        /* obj->obj_discard == false */
                        rc = vos_ilog_update(cont, &obj->obj_df->vo_ilog, epr, bound, NULL, &obj->obj_ilog_info, cond_mask, ts_set);
                                /* parent == NULL */
                                /** Do a fetch first.  The log may already exist */
                                rc = vos_ilog_fetch(vos_cont2umm(cont), vos_cont2hdl(cont), DAOS_INTENT_UPDATE, ilog, epr->epr_hi, bound, has_cond, NULL, parent, info);
                                        vos_ilog_fetch_internal(umm, coh, intent, ilog, &epr, bound, has_cond, punched, parent, info);
                                                vos_ilog_desc_cbs_init(&cbs, coh);
                                                ilog_fetch(umm, ilog, &cbs, intent, has_cond, &info->ii_entries);
                                                        ilog_fetch_cached(umm, root, cbs, intent, has_cond, entries)
                                                                return false;
                                                        /* ilog_empty(root) == false */
                                                        ilog_log2cache(lctx, &cache);
                                                                /* ilog_empty(lctx->ic_root) == false */
                                                                /* lctx->ic_root->lr_tree.it_embedded == true */ 
                                                        rc = prepare_entries(entries, &cache);
                                                                /* cache->ac_nr <= NUM_EMBEDDED */
                                                        for (i = 0; i < cache.ac_nr; i++) {
                                                                status = ilog_status_get(lctx, id, intent, retry);
                                                                        vos_ilog_status_get /* cbs->dc_log_status_cb(lctx->ic_umm, id->id_tx_id, id->id_epoch, intent, retry, cbs->dc_log_status_args);*/
                                                                                vos_dtx_check_availability(coh, tx_id, epoch, intent = DAOS_INTENT_UPDATE, type = DTX_RT_ILOG, retry);
                                                                                        /* dth != NULL */
                                                                                        switch (type) {
                                                                                        case DTX_RT_ILOG:
                                                                                                break;
                                                                                        /* intent != DAOS_INTENT_CHECK */
                                                                                        dtx_is_committed(entry, cont, epoch)
                                                                                /* rc == ALB_AVAILABLE_CLEAN */
                                                                        /* rc == ILOG_COMMITTED */
                                                                /* status == 1 */
                                                        /* entries->ie_num_entries != 0 */
                                                /* rc == 0 */
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
                                                        info->ii_prior_punch = *punch;
                                                        /* vos_epc_punched(info->ii_prior_any_punch.pr_epc, info->ii_prior_any_punch.pr_minor_epc, punch) == true */
                                                        info->ii_prior_any_punch = *punch;
                                                /* rc == 0 */
                                                rc = vos_ilog_update_check(info, &max_epr);
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
                        rc = key_tree_prepare(obj, obj->obj_toh, VOS_BTR_DKEY, dkey, SUBTR_CREATE, DAOS_INTENT_UPDATE, &krec, &ak_toh, ioc->ic_ts_set);
                                /* krecp != NULL */
                                *krecp = NULL;
                                rc = dbtree_fetch(toh, BTR_PROBE_EQ, intent, key, NULL, &riov);
                                        rc = btr_verify_key(tcx, key);
                                        /* rc == 0 */
                                        rc = btr_probe_key(tcx, opc, intent, key);
                                                /* XXX */
                                        /* rc == PROBE_RC_OK */
                                        rec = btr_trace2rec(tcx, tcx->tc_depth - 1);
                                        btr_rec_fetch(tcx, rec, key_out, val_out);
                                                ktr_rec_fetch /* btr_ops(tcx)->to_rec_fetch(&tcx->tc_tins, rec, key, val); */
                                /* rc == 0 */
                                /* ilog != NULL && (flags & SUBTR_CREATE) */
                                vos_ilog_ts_ignore(vos_obj2umm(obj), &krec->kr_ilog);
                                        /* !DAOS_ON_VALGRIND */
                                tmprc = vos_ilog_ts_add(ts_set, ilog, key->iov_buf, (int)key->iov_len);
                                        /* vos_ts_in_tx(ts_set) == true */
                                        /* ilog != NULL */
                                        idx = ilog_ts_idx_get(ilog);
                                        vos_ts_set_add(ts_set, idx, record, rec_size);
                                               /* XXX */ 
                                /* tmprc == 0 */
                                /* sub_toh != NULL */
                                rc = tree_open_create(obj, tclass, flags, krec, created, sub_toh);
                                        /* !(flags & SUBTR_EVT) */
                                        dbtree_open_inplace_ex(&krec->kr_btr, uma, coh, pool, sub_toh);
                                                rc = btr_context_create(BTR_ROOT_NULL, root, -1, -1, -1, uma, coh, priv, &tcx);
                                                        rc = btr_class_init(root_off, root, tree_class, &tree_feats, uma, coh, priv, &tcx->tc_tins);
                                                                rc = umem_class_init(uma, &tins->ti_umm);
                                                                        set_offsets(umm);
                                                                                switch (umm->umm_id) {
                                                                                case UMEM_CLASS_PMEM:
                                                                                        root_oid = pmemobj_root(pop, 0);
                                                                                        root = pmemobj_direct(root_oid);
                                                                /* tc->tc_feats & BTR_FEAT_DYNAMIC_ROOT */
                                                                *tree_feats |= BTR_FEAT_DYNAMIC_ROOT;
                                                        /* rc == 0 */
                                                        btr_context_set_depth(tcx, depth);
                                                /* rc == 0 */
                                /* rc == 0 */
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
