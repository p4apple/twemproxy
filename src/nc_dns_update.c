/*
 * nc_dns_update.c
 *
 *  Created on: Sep 20, 2018
 *      Author: wangxin
 */

#include <unistd.h>
#include <nc_dns_update.h>
#include <nc_core.h>
#include <nc_util.h>

int compare_socker_info( struct sockinfo * src_si , int src_si_num ,
		struct sockinfo * target_si , int target_si_num  ){
	if(src_si == NULL || target_si == NULL ){
		return -1;
	}
	if( src_si_num != target_si_num ){
		return -1 ;
	}
	int i,j ;
	for(i = 0 ; i < src_si_num ; i++  ){
		int match = -1 ;
		for(j = 0 ; j < target_si_num ; j++  ){
			if(src_si[i].family == target_si[j].family &&
					src_si[i].addrlen == target_si[j].addrlen &&
					memcmp(&(src_si[i].addr) ,
							&(target_si[j].addr),src_si[i].addrlen) == 0){
				match = 1;
				break ;

			}
		}
		if(match != 1 ){
			return -1 ;
		}
	}

	return 0 ;
}

int check_domain_name_ip( struct server * srv ){

	int i ;
	int socketinfo_num = 0;
	struct sockinfo * si = NULL;
    if (srv->pname.data[0] != '/') {


		int irt = nc_get_addr_info(&srv->addrstr, srv->port, &si,
				&socketinfo_num);
		if (irt == -1) {
			//log_debug(LOG_VVERB, "add:%s prot:%d " , srv->addrstr.data, srv->port);
			log_error("Failed when call nc_get_addr_info function . addr:%s&%d." , srv->addrstr.data, srv->port);
		} else {

			if(socketinfo_num > 0 && si != NULL &&
					compare_socker_info( srv->dns_info_pool , srv->dns_info_num ,
					si  , socketinfo_num ) != 0 ) {
				//log_debug(LOG_VVERB, "add:%s prot:%d compare != 0 " , srv->addrstr.data, srv->port);
				log_error("DNS has changed. addr:%s&%d ip num:%d/%d (new/old) ." , srv->addrstr.data, srv->port ,
						socketinfo_num  ,srv->dns_info_num  );
				if(srv->last_dns_info_pool != NULL){
					nc_free_addr_info(&srv->last_dns_info_pool);
					srv->last_dns_info_pool = 0 ;
				}
				srv->last_dns_info_pool = si ;
				srv->last_dns_info_num = socketinfo_num ;

				for( i = 0 ; i < socketinfo_num ; i++  ){
					print_log_addr_info(si + i );
				}

				return -1 ;

			}else{
				nc_free_addr_info(&si);
				socketinfo_num = 0 ;
				log_debug(LOG_VVERB, "add:%s prot:%d compare == 0  " , srv->addrstr.data, srv->port);
			}
		}
	}
    return 0 ;

}


static void *
dns_check(  void *arg ){

	struct context *ctx = arg ;

	while (1) {
		uint32_t i, j, npool, nserver;

		npool = array_n(&ctx->pool);

		log_debug(LOG_VVERB, "**** start check service_pool: %d " , npool );
		if (ctx->dns_update_state == 0) {
			uint32_t have_dns_update = 0 ;
			for (i = 0; i < npool; i++) {
				struct server_pool *sp = array_get(&ctx->pool, i);

				log_debug(LOG_VVERB, "server_pool: %s = %s " , sp->name.data , sp->addrstr.data );

				nserver = array_n(&sp->server);

				for (j = 0; j < nserver; j++) {

					struct server * sv = array_get(&sp->server, j);

					if (sv->dns_update_state == 0) {

						if( check_domain_name_ip(sv)!= 0){
							sv->dns_update_state = 1;
							have_dns_update = 1 ;
						}
						//log_debug(LOG_VVERB, "  |---server dns updateing %s : %d : %d " , sv->addrstr.data , sv->port , sv->ns_conn_q );
						//sleep(2);

					} else {
						//log_debug(LOG_VVERB, "  |---server %s : %d : %d " , sv->addrstr.data , sv->port  , sv->ns_conn_q );
					}
				}
			}

			if(have_dns_update == 1 ){
				ctx->dns_update_state  = 1 ;
			}
		}
		sleep(1);
	}

	return NULL ;
}

rstatus_t
start_dns_update(  struct context *ctx ){

	log_debug(LOG_VVERB, "start start dns update . " );

	rstatus_t status;
	pthread_t           tid;

    status = pthread_create(&tid, NULL, dns_check, ctx);
    if (status < 0) {
        log_error("stats aggregator create failed: %s", strerror(status));
        return NC_ERROR;
    }

    return NC_OK;

}
