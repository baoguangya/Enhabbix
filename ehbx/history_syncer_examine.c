/** 该函数位于src/libs/zbxdbcache/dbcache.c中  **/
static void	hc_add_item_values(dc_item_value_t *values, int values_num)
{
	dc_item_value_t	*item_value;
	int		i;
	zbx_hc_item_t	*item;

	for (i = 0; i < values_num; i++)
	{
		zbx_hc_data_t	*data = NULL;

		item_value = &values[i];

		while (SUCCEED != hc_clone_history_data(&data, item_value))
		{
			UNLOCK_CACHE;

			zabbix_log(LOG_LEVEL_DEBUG, "History cache is full. Sleeping for 1 second.");
			sleep(1);

			LOCK_CACHE;
		}

		if (NULL == (item = hc_get_item(item_value->itemid)))
		{
			item = hc_add_item(item_value->itemid, data);
			hc_queue_item(item);
		}
		else
		{
			item->head->next = data;
			item->head = data;
		}
		
		//添加以下语句块
		switch (data->value_type)
		{
			case ITEM_VALUE_TYPE_FLOAT:
				zabbix_log(LOG_LEVEL_DEBUG, "pushing into history_queue: itemid [%d] value [%f]",item_value->itemid,item_value->value.value_dbl);
				break;
			case ITEM_VALUE_TYPE_UINT64:
				zabbix_log(LOG_LEVEL_DEBUG, "pushing into history_queue: itemid [%d] value [%d]",item_value->itemid,item_value->value.value_uint);
				break;
			default:
				zabbix_log(LOG_LEVEL_DEBUG, "pushing into history_queue: itemid [%d] value [%s]",item_value->itemid,item_value->value.value_str);
		}
		
	}
}

/** 该函数位于src/libs/zbxdbcache/dbcache.c中  **/
static void     hc_pop_items(zbx_vector_ptr_t *history_items)
{
        zbx_binary_heap_elem_t  *elem;
        zbx_hc_item_t           *item;
        zbx_hc_data_t   *ehbx_value;
        size_t ehbx_popmsg_alloc=0, ehbx_popmsg_offset=0;
        char *ehbx_popmsg=NULL;

        while (ZBX_HC_SYNC_MAX > history_items->values_num && FAIL == zbx_binary_heap_empty(&cache->history_queue))
        {
                elem = zbx_binary_heap_find_min(&cache->history_queue);
                item = (zbx_hc_item_t *)elem->data;
                zbx_vector_ptr_append(history_items, item);
                ehbx_value = item->tail;

                zbx_snprintf_alloc(&ehbx_popmsg, &ehbx_popmsg_alloc, &ehbx_popmsg_offset, "poping values of ");

                while (NULL != ehbx_value)
                {
                        if (ehbx_value->value_type == ITEM_VALUE_TYPE_TEXT){
                        zbx_snprintf_alloc(&ehbx_popmsg, &ehbx_popmsg_alloc, &ehbx_popmsg_offset, "itemid [%d] value [%s] ts [%d:%d]-->",
                                item->itemid,zbx_strdup(NULL, ehbx_value->value.str), ehbx_value->ts.sec, ehbx_value->ts.ns);
                        }
                        ehbx_value = ehbx_value->next;

                }
                zabbix_log(LOG_LEVEL_DEBUG,"%s NULL",ehbx_popmsg);
                zbx_free(ehbx_popmsg);
                ehbx_popmsg_alloc = ehbx_popmsg_offset = 0;
                zbx_binary_heap_remove_min(&cache->history_queue);
        }
}

/** 该函数位于src/libs/zbxdbcache/dbcache.c中  **/
void    hc_push_items(zbx_vector_ptr_t *history_items)
{
        int             i;
        zbx_hc_item_t   *item;
        zbx_hc_data_t   *data_free;
        zbx_hc_data_t   *ehbx_value;
        char *ehbx_pushmsg = NULL;
        size_t ehbx_pushmsg_alloc=0, ehbx_pushmsg_offset=0;

        for (i = 0; i < history_items->values_num; i++)
        {
                item = (zbx_hc_item_t *)history_items->values[i];
                ehbx_value = item->tail;

                zbx_snprintf_alloc(&ehbx_pushmsg, &ehbx_pushmsg_alloc, &ehbx_pushmsg_offset, "pushing values of ");

                while (NULL != ehbx_value)
                {
                        if (ehbx_value->value_type == ITEM_VALUE_TYPE_TEXT){
                        zbx_snprintf_alloc(&ehbx_pushmsg, &ehbx_pushmsg_alloc, &ehbx_pushmsg_offset, "itemid [%d] value [%s] valuetype [%d] ts [%d:%d]-->",
                                item->itemid, zbx_strdup(NULL, ehbx_value->value.str),ehbx_value->value_type,ehbx_value->ts.sec,ehbx_value->ts.ns);
                        }
                        ehbx_value = ehbx_value->next;
                }
                zabbix_log(LOG_LEVEL_DEBUG,"%s NULL",ehbx_pushmsg);
                zbx_free(ehbx_pushmsg);
                ehbx_pushmsg_alloc = ehbx_pushmsg_offset = 0;

                switch (item->status)
                {
                        case ZBX_HC_ITEM_STATUS_BUSY:
                                /* reset item status before returning it to queue */
                                item->status = ZBX_HC_ITEM_STATUS_NORMAL;
                                hc_queue_item(item);
                                break;
                        case ZBX_HC_ITEM_STATUS_NORMAL:
                                data_free = item->tail;
                                item->tail = item->tail->next;
                                hc_free_data(data_free);
                                if (NULL == item->tail)
                                        zbx_hashset_remove(&cache->history_items, item);
                                else
                                        hc_queue_item(item);
                                break;
                }
        }
}