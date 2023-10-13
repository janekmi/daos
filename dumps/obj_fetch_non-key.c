/* fetch object of non-existing dkey after successfull sequence of update - fetch w/o transaction */
vos_obj_fetch -> vos_obj_fetch_ex
        vos_fetch_begin
                vos_ioc_create(…, &ioc)
                        vos_ioc_reserve_init /* if (!ioc->ic_update) return 0; */
                        if(dtx_is_valid_handle(NULL)) //false

                        vos_ts_set_allocate
                        bioc = vos_data_ioctxt (return vp->vp_dummy_ioctxt;)
                        ioc->ic_biod = bio_iod_alloc(bioc, …)
                        dcs_csum_info_list_init
                        for (i = 0; i < iod_nr; i++) {
                                bsgl = bio_iod_sgl(ioc->ic_biod, i);
                                bio_sgl_init(bsgl, iov_nr);
                vos_dth_set -> tls->vtl_dth = NULL;
                vos_ts_set_add(ioc->ic_ts_set, ioc->ic_cont->vc_ts_idx, NULL, 0); (if (!vos_ts_in_tx(ts_set)) return 0;)
                occ = vos_obj_cache_current()
                        vos_obj_cache_get (return vos_tls_get(standalone)->vtl_ocache;)
                vos_obj_hold(occ, …)
                        daos_lru_ref_hold(lcache = occ, …)
                                link = d_hash_rec_find(&lcache->dlc_htable, key, key_size);
                                        idx = ch_key_hash(htable, key, ksize);
                                        bucket = &htable->ht_buckets[idx];
                                        ch_bucket_lock(htable, idx, !is_lru);
                                        link = ch_rec_find(...)
                                        /* !!! */
                                        if (link != NULL)
                                                ch_rec_addref(htable, link);
                                        ch_bucket_unlock(htable, idx, !is_lru);
                                if (link != NULL) {
                                        llink = link2llink(link);
                                         if (!d_list_empty(&llink->ll_qlink))
                                                d_list_del_init(&llink->ll_qlink);
                        /* rc == 0 */
                        /** Object is in cache */
                        obj = container_of(lret, struct vos_object, obj_llink);
                        if (obj->obj_df) { /* Persistent memory address of the object */
                                tmprc = vos_ilog_ts_add(ts_set, &obj->obj_df->vo_ilog, &oid, sizeof(oid)); /* if (!vos_ts_in_tx(ts_set)) return 0;*/
                                goto check_object
                        if (!create) {
                                vos_ilog_fetch
                                        vos_ilog_fetch_internal(umm, coh, intent, ilog, &epr, bound, has_cond, punched, parent, info);
                                                vos_ilog_desc_cbs_init(&cbs, coh);
                                                ilog_fetch(umm, ilog, &cbs, intent, has_cond, &info->ii_entries);
                                                        if(ilog_fetch_cached(umm, root, cbs, intent, has_cond, entries)) true
                                                                ilog_status_refresh()
                                                                return true /* DIFFERENT */
                                                        return 0 (rc);

                                                /* !ilog_empty(root)) */
#if 0 // It is skipped
                                                ilog_log2cache(lctx, &cache);
                                                prepare_entries(entries, &cache);
                                                        entries->ie_ids = cache->ac_entries;
                                                for (i = 0; i < cache.ac_nr; i++) {
                                                        id = &cache.ac_entries[i];
                                                        status = ilog_status_get(lctx, id, intent, retry);
                                                                vos_ilog_status_get /* cbs->dc_log_status_cb */
                                                                        rc = vos_dtx_check_availability(coh, tx_id, epoch, intent, DTX_RT_ILOG, retry);
                                                                                dth = vos_dth_get(cont->vc_pool->vp_sysdb);
                                                                                if (dtx_is_committed(entry, cont, epoch))
                                                                        /* rc == ALB_AVAILABLE_CLEAN */
                                                        /* status == ILOG_COMMITTED */
                                                        entries->ie_info[entries->ie_num_entries].ii_removed = 0;
                                                        entries->ie_info[entries->ie_num_entries++].ii_status = status;
                                                /* entries->ie_num_entries == 1 */
                                                priv->ip_rc = rc; /* 0 */
#endif
                                                vos_parse_ilog(info, epr, bound, &punch); TG
                                                        ilog_foreach_entry_reverse(&info->ii_entries, &entry) {
                                                                /* entry.ie_status != ILOG_REMOVED */
                                                                /* !vos_ilog_punched(&entry, punch)) */
                                                                entry_epc = entry.ie_id.id_epoch; /* 5 */
                                                                /* if (entry_epc > epr->epr_hi) { *//* {epr_lo = 0, epr_hi = 6} */ ! 5 > 6
                                                                /* if (entry.ie_status == -DER_INPROGRESS) FALSE */
                                                                /* !vos_ilog_punch_covered(&entry, &info->ii_prior_any_punch) */
                                                                /* if (entry.ie_status == ILOG_UNCOMMITTED) == FALSE /* ie_status == -138768134 */ */
                                                                if (entry.ie_id.id_epoch > info->ii_uncommitted) /* 9975412875014583296 > 0 */
                                                                        info->ii_uncommitted = 0;
                                                                /* entry.ie_status == ILOG_COMMITTED */
                                                                /* if (ilog_has_punch(&entry)) */
                                                                info->ii_create = entry.ie_id.id_epoch; /* 9975412875014583296 */
                                                        /* epr->epr_lo == 0 */
                                                        if (vos_epc_punched(info->ii_prior_punch.pr_epc, info->ii_prior_punch.pr_minor_epc, punch))
                                                                info->ii_prior_punch = *punch;
                                                        if (vos_epc_punched(info->ii_prior_any_punch.pr_epc, info->ii_prior_any_punch.pr_minor_epc, punch))
                                                                info->ii_prior_any_punch = *punch;
                                                        return 0;
                                /* rc == 0 */
                                vos_ilog_check(&obj->obj_ilog_info, epr, epr, visible_only);
                                        if (visible_only) {
                                                if (epr_out && epr_out->epr_lo < info->ii_create)
                                                        epr_out->epr_lo = info->ii_create;
                                /* rc == 0 */ => goto out
                                if (obj->obj_df != NULL)
                                        obj->obj_sync_epoch = obj->obj_df->vo_sync; /* 0 */
                                /* intent == DAOS_INTENT_DEFAULT */
                                *obj_p = obj;
                                return 0;
                        /* rc == 0 */
                        if (stop_check(ioc, VOS_COND_FETCH_MASK | VOS_OF_COND_PER_AKEY, NULL, &rc, false)) { /* if (*rc == 0) return false; */
                        /* dkey->iov_len == 6 */
                        => goto fetch_dkey;
                        dkey_fetch(ioc, dkey);
                                obj_tree_init(obj);
                                /* rc == 0 */
                                key_tree_prepare(tclass = VOS_BTR_DKEY, krecp = &krec, sub_toh = &toh)
                                        vos_kh_clear /* reset the saved hash */
                                        if (krecp != NULL)
                                                *krecp = NULL;

                                        dbtree_fetch(toh, BTR_PROBE_EQ, intent, key, NULL, &riov);
                                                tcx = btr_hdl2tcx(toh);
                                                btr_verify_key(tcx, key);
                                                rc = btr_probe_key(tcx, opc, intent, key);
                                                /* rc == PROBE_RC_NONE */
                                                return -DER_NONEXIST
                                        /* rc == -DER_NONEXIST */
                                        /* krec = rbund.rb_krec;
                                           ilog = &krec->kr_ilog; */
                                        case -DER_NONEXIST:
                                        tmprc = vos_ilog_ts_add(ts_set, ilog, key->iov_buf, (int)key->iov_len); /* if (!vos_ts_in_tx(ts_set)) return 0;*/
                                        tmprc == 0 /* !!! */
                                	if (rc == -DER_NONEXIST) 
                                       		if (!(flags & SUBTR_CREATE))
                        			goto out;
                                        return -DER_NONEXIST; 
                                        /* rc == -1005 */
                                stop_check()
                                        if (ioc->ic_ts_set == NULL) {
	                                        *rc = 0; /*?????? */
		                                return true;
	                                }
                                        iod_empty_sgl()
                                        goto out
                                        return 0
                vos_fetch_add_missing(ioc->ic_ts_set, dkey, iod_nr, iods); return as ts_set == NULL
/** Add missing timestamp cache entries.  This should be called
 *  when execution may have been short circuited by a non-existent
 *  entity so we can fill in the negative timestamps before doing
 *  timestamp updates.
 */
                vos_ts_set_update(ioc->ic_ts_set, ioc->ic_epr.epr_hi); return as ts_set == NULL
                return 0;
        if (!size_fetch) {
        for (i = 0; i < iod_nr; i++) { //iod_nr == 5
                bio_iod_sgl(struct bio_desc *biod, unsigned int idx)
                if (bsgl->bs_nr_out == 0) { /* Inform caller the nonexistent of object/key */
                for (j = 0; j < sgl->sg_nr; j++)
		        sgl->sg_iovs[j].iov_len = 0; }}
                vos_obj_copy(ioc, sgls, iod_nr) /* why ???? ?*/
                        iod_prep -> iod_prep_internal
                                iod_map_iovs
                                        iod_dma_buf
                                        iod_fifo_in
                                        iterate_biov(struct bio_desc *biod, int (*cb_fn)(struct bio_desc *, struct bio_iov *, void *data), void *data)
                                	        for (i = 0; i < biod->bd_sgl_cnt; i++) {
                                                      /* data == NULL */
                                                        if (bsgl->bs_nr_out == 0)
                                                	        continue;
                                        return rc /* rc == 0 */
                               	        biod->bd_buffer_prep = 1;
                                        iod_fifo_out(biod, bdb);
                                                if (!biod->bd_in_fifo) 
                                                return;
                                        return 0;
                                /* All direct SCM access, no DMA buffer prepared */
	                        if (biod->bd_rsrvd.brd_rg_cnt == 0)
		                        return 0;
                        rc = bio_iod_copy(ioc->ic_biod, sgls, sgl_nr);
                                iterate_biov(biod, copy_one, &arg);
                                	for (i = 0; i < biod->bd_sgl_cnt; i++) { /* biod->bd_sgl_cnt == 1 */
                                                struct bio_sglist *bsgl = &biod->bd_sgls[i];
                                                if (data != NULL) {
                                                        if (cb_fn == copy_one) {
                                                                ...
                                                        }}}
                                        return 0;
                        rc = bio_iod_post(ioc->ic_biod, rc);
                                /* No more actions for direct accessed SCM IOVs */
                                if (biod->bd_rsrvd.brd_rg_cnt == 0) {
                                        iod_release_buffer(biod);
                                                bulk_iod_release(biod);
                                                        /* No reserved DMA regions */
                                                        if (rsrvd_dma->brd_rg_cnt == 0) {
                                                                biod->bd_buffer_prep = 0;
                                                                return;}
                                 return 0;
                        return 0;
        rc = vos_fetch_end(ioh, NULL, rc);
                vos_ioc_destroy(ioc, false)
                        vos_ioc_reserve_fini(ioc);
                        vos_ilog_fetch_finish(&ioc->ic_dkey_info);
                        vos_ilog_fetch_finish(&ioc->ic_akey_info);
                        vos_cont_decref(ioc->ic_cont);
                        vos_ts_set_free(ioc->ic_ts_set);
                return 0
        return 0





#if 0
                                        if (sub_toh)
                                                tree_open_create(sub_toh)
                                                        vos_evt_desc_cbs_init(&cbs, pool, coh);
                                                        if (krec->kr_bmap & expected_flag) {
                                                                /* !(flags & SUBTR_EVT) */
                                                                        dbtree_open_inplace_ex(&krec->kr_btr, uma, coh, pool, sub_toh);
                                                                                btr_context_create(BTR_ROOT_NULL, root, -1, -1, -1, uma, coh, priv, &tcx);
                                                                                /* rc == 0 */
                                                                                *toh = btr_tcx2hdl(tcx);
                                                                        /* rc == 0 */

                                        if (krecp != NULL)
                                                *krecp = krec;
#endif
