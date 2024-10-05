// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>

#include "m4u_debug.h"
#include "m4u_priv.h"

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
#include "trusted_mem_api.h"
#endif

/* global variables */
int gM4U_log_to_uart = 2;
int gM4U_log_level = 2;

#if IS_ENABLED(CONFIG_DEBUG_FS) || IS_ENABLED(CONFIG_PROC_FS)
unsigned int gM4U_seed_mva;

int m4u_test_alloc_dealloc(int id, unsigned int size)
{
	return 0;
}

m4u_callback_ret_t m4u_test_callback(int alloc_port, unsigned int mva,
				     unsigned int size, void *data)
{
	return 0;
}

int m4u_test_reclaim(unsigned int size)
{
	return 0;
}

int __attribute__((weak)) ddp_mem_test(void)
{
	return 0;
}

int __attribute__((weak)) __ddp_mem_test(unsigned int *pSrc,
		unsigned int pSrcPa, unsigned int *pDst,
		unsigned int pDstPa, int need_sync)
{
	return 0;
}

int m4u_test_ddp(unsigned int prot)
{
	return 0;
}

m4u_callback_ret_t test_fault_callback(int port, unsigned int mva, void *data)
{
	return M4U_CALLBACK_HANDLED;
}

int m4u_test_tf(unsigned int prot)
{
	return 0;
}

#ifdef CONFIG_M4U_TEST_ION
#include <mtk/ion_drv.h>

void m4u_test_ion(void)
{
	unsigned int *pSrc, *pDst;
	unsigned long src_pa, dst_pa;
	unsigned int size = 64 * 64 * 3, tmp_size;
	struct m4u_port_config_struct port;
	struct ion_mm_data mm_data;
	struct ion_client *ion_client;
	struct ion_handle *src_handle, *dst_handle;

	/* FIX-ME: modified for linux-3.10 early porting */
	/* ion_client = ion_client_create(g_ion_device, 0xffffffff, "test"); */
	ion_client = ion_client_create(g_ion_device, "test");

	src_handle =
		ion_alloc(ion_client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
	dst_handle =
		ion_alloc(ion_client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);

	pSrc = ion_map_kernel(ion_client, src_handle);
	pDst = ion_map_kernel(ion_client, dst_handle);

	mm_data.config_buffer_param.kernel_handle = src_handle;
	mm_data.config_buffer_param.eModuleID = M4U_PORT_DISP_OVL0;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
	    (unsigned long)&mm_data) < 0)
		m4u_err("ion_test_drv: Config buffer failed.\n");

	mm_data.config_buffer_param.kernel_handle = dst_handle;
	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
	    (unsigned long)&mm_data) < 0)
		m4u_err("ion_test_drv: Config buffer failed.\n");

	ion_phys(ion_client, src_handle, &src_pa, (size_t *)&tmp_size);
	ion_phys(ion_client, dst_handle, &dst_pa, (size_t *)&tmp_size);

	m4u_err("ion alloc: pSrc=0x%p pDst=0x%p src_pa=%lu, dst_pa=%lu\n",
		pSrc, pDst, src_pa, dst_pa);

	port.ePortID = M4U_PORT_DISP_OVL0;
	port.Direction = 0;
	port.Distance = 1;
	port.domain = 3;
	port.Security = 0;
	port.Virtuality = 1;
	m4u_config_port(&port);

	port.ePortID = M4U_PORT_DISP_WDMA0;
	m4u_config_port(&port);

	m4u_monitor_start(0);
	__ddp_mem_test(pSrc, src_pa, pDst, dst_pa, 0);
	m4u_monitor_stop(0);

	ion_free(ion_client, src_handle);
	ion_free(ion_client, dst_handle);

	ion_client_destroy(ion_client);
}
#else
#define m4u_test_ion(...)
#endif

static int m4u_debug_set(void *data, u64 val)
{
#if 0
	struct m4u_domain *domain = data;

	m4u_err("%s:val=%llu\n", __func__, val);

	switch (val) {
	case 1:
	{
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		struct page *page;
		int page_num = 512;
		unsigned int mva = 0x4000;

		page = alloc_pages(GFP_KERNEL, get_order(page_num));
		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
			sg_set_page(sg, page + i, PAGE_SIZE, 0);
		m4u_map_sgtable(domain, mva, sg_table,
				page_num * PAGE_SIZE,
				M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, page_num * PAGE_SIZE);
		m4u_dump_pgtable(domain, NULL);

		sg_free_table(sg_table);
		__free_pages(page, get_order(page_num));
	}
	break;
	case 2:
	{                   /* map64kpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		int page_num = 51;
		unsigned int page_size = SZ_64K;
		unsigned int mva = SZ_64K;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}

		m4u_map_sgtable(domain, mva, sg_table,
				page_num * page_size,
				M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, page_num * page_size);
		m4u_dump_pgtable(domain, NULL);
		sg_free_table(sg_table);
	}
	break;
	case 3:
	{                   /* map1Mpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		int page_num = 37;
		unsigned int page_size = SZ_1M;
		unsigned int mva = SZ_1M;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);

		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}
		m4u_map_sgtable(domain, mva, sg_table,
				page_num * page_size,
				M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, page_num * page_size);
		m4u_dump_pgtable(domain, NULL);

		sg_free_table(sg_table);
		}
		break;
	case 4:
	{                   /* map16Mpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		int page_num = 2;
		unsigned int page_size = SZ_16M;
		unsigned int mva = SZ_16M;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}
		m4u_map_sgtable(domain, mva, sg_table,
				page_num * page_size,
				M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, page_num * page_size);
		m4u_dump_pgtable(domain, NULL);
		sg_free_table(sg_table);
		}
		break;
	case 5:
	{                   /* mapmiscpages */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		unsigned int mva = 0x4000;
		unsigned int size = SZ_16M * 2;

		sg_alloc_table(sg_table, 1, GFP_KERNEL);
		sg = sg_table->sgl;
		sg_dma_address(sg) = 0x4000;
		sg_dma_len(sg) = size;

		m4u_map_sgtable(domain, mva, sg_table, size,
				M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, size);
		m4u_dump_pgtable(domain, NULL);
		sg_free_table(sg_table);
	}
	break;
	case 6:
		m4u_test_alloc_dealloc(1, SZ_4M);
		break;
	case 7:
		m4u_test_alloc_dealloc(2, SZ_4M);
		break;
	case 8:
		m4u_test_alloc_dealloc(3, SZ_4M);
		break;
	case 9:                /* m4u_alloc_mvausingkmallocbuffer */
	{
		m4u_test_reclaim(SZ_16K);
		m4u_mvaGraph_dump();
	}
	break;
	case 10:
	{
		unsigned int mva;

		mva = m4u_do_mva_alloc_fix(0, 0x90000000, 0x10000000, NULL);
		m4u_info("mva alloc fix done:mva=0x%x\n", mva);
		mva = m4u_do_mva_alloc_fix(0, 0xb0000000, 0x10000000, NULL);
		m4u_info("mva alloc fix done:mva=0x%x\n", mva);
		mva = m4u_do_mva_alloc_fix(0, 0xa0000000, 0x10000000, NULL);
		m4u_info("mva alloc fix done:mva=0x%x\n", mva);
		mva = m4u_do_mva_alloc_fix(0, 0xa4000000, 0x10000000, NULL);
		m4u_info("mva alloc fix done:mva=0x%x\n", mva);
		m4u_mvaGraph_dump();
		m4u_do_mva_free(0x90000000, 0x10000000);
		m4u_do_mva_free(0xa0000000, 0x10000000);
		m4u_do_mva_free(0xb0000000, 0x10000000);
		m4u_mvaGraph_dump();
	}
	break;
	case 11:    /* map unmap kernel */
		m4u_test_map_kernel();
		break;
	case 12:
		ddp_mem_test();
		break;
	case 13:
		m4u_test_ddp(M4U_PROT_READ|M4U_PROT_WRITE);
		break;
	case 14:
		m4u_test_tf(M4U_PROT_READ|M4U_PROT_WRITE);
		break;
	case 15:
		m4u_test_ion();
		break;
	case 16:
		m4u_dump_main_tlb(0, 0);
		break;
	case 17:
		m4u_dump_pfh_tlb(0);
		break;
	case 18:
	{
		if (TOTAL_M4U_NUM > 1)
			m4u_dump_main_tlb(1, 0);
		break;
	}
	case 19:
	{
		if (TOTAL_M4U_NUM > 1)
			m4u_dump_pfh_tlb(1);
		break;
	}
	case 20:
	{
		struct m4u_port_config_struct rM4uPort;
		int i;

		rM4uPort.Virtuality = 1;
		rM4uPort.Security = 0;
		rM4uPort.Distance = 1;
		rM4uPort.Direction = 0;
		rM4uPort.domain = 3;
		for (i = 0; i < M4U_PORT_UNKNOWN; i++) {
			rM4uPort.ePortID = i;
			m4u_config_port(&rM4uPort);
		}
	}
	break;
	case 21:
	{
		struct m4u_port_config_struct rM4uPort;
		int i;

		rM4uPort.Virtuality = 0;
		rM4uPort.Security = 0;
		rM4uPort.Distance = 1;
		rM4uPort.Direction = 0;
		rM4uPort.domain = 3;
		for (i = 0; i < M4U_PORT_UNKNOWN; i++) {
			rM4uPort.ePortID = i;
			m4u_config_port(&rM4uPort);
		}
	}
	break;
	case 22:
		m4u_info("cache sync not support, no need test!\n");
		break;
	case 23:
	{
		void *pgd_va;
		void *pgd_pa;
		unsigned int size;

		m4u_get_pgd(NULL, 0, &pgd_va, &pgd_pa, &size);
		m4u_err("pgd_va:0x%p pgd_pa:0x%p, size: %d\n",
			pgd_va, pgd_pa, size);
	}
	break;
	case 24:
	{
		unsigned int *pSrc;
		unsigned int mva = 0;
		unsigned long pa;
		struct m4u_client_t *client = m4u_create_client();

		pSrc = vmalloc(128);
		m4u_alloc_mva(client, M4U_PORT_DISP_OVL0,
			      (unsigned long)pSrc, NULL, 128, 0, 0, &mva);

		m4u_dump_pgtable(domain, NULL);

		pa = m4u_mva_to_pa(NULL, 0, mva);
		m4u_err("(1) mva:0x%x pa:0x%lx\n", mva, pa);
			m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, mva);
		pa = m4u_mva_to_pa(NULL, 0, mva);
		m4u_err("(2) mva:0x%x pa:0x%lx\n", mva, pa);
		m4u_destroy_client(client);
		vfree(pSrc);
	}
	break;
	case 25:
		m4u_monitor_start(0);
		break;
	case 26:
		m4u_monitor_stop(0);
		break;
	case 27:
		/*m4u_dump_reg_for_smi_hang_issue();*/
		break;
	case 28:
	{
#if 0
		unsigned char *pSrc;
		unsigned char *pDst;
		unsigned int mva_rd;
		unsigned int mva_wr;
		unsigned int allocated_size = 1024;
		unsigned int i;
		struct m4u_client_t *client = m4u_create_client();

		m4u_monitor_start(0);

		pSrc = vmalloc(allocated_size);
		memset(pSrc, 0xFF, allocated_size);
		m4u_err("(0) vmalloc pSrc:0x%p\n", pSrc);
		pDst =  vmalloc(allocated_size);
		memset(pDst, 0xFF, allocated_size);
		m4u_err("(0) vmalloc pDst:0x%p\n", pDst);
		m4u_err("(1) pDst check 0x%x 0x%x 0x%x 0x%x  0x%x\n",
			*pDst, *(pDst+1), *(pDst+126),
			*(pDst+127), *(pDst+128));


		m4u_alloc_mva(client, M4U_PORT_DISP_FAKE_LARB0,
			      (unsigned long)pSrc, NULL,
			      allocated_size, 0, 0, &mva_rd);
		m4u_alloc_mva(client, M4U_PORT_DISP_FAKE_LARB0,
			      (unsigned long)pDst, NULL,
			      allocated_size, 0, 0, &mva_wr);

		m4u_dump_pgtable(domain, NULL);

		m4u_display_fake_engine_test(mva_rd, mva_wr);

		m4u_err("(2) mva_wr:0x%x\n", mva_wr);

		m4u_dealloc_mva(client, M4U_PORT_DISP_FAKE_LARB0, mva_rd);
		m4u_dealloc_mva(client, M4U_PORT_DISP_FAKE_LARB0, mva_wr);

		m4u_cache_sync(NULL, 0, 0, 0, 0, M4U_CACHE_FLUSH_ALL);

		m4u_destroy_client(client);

		m4u_err("(3) pDst check 0x%x 0x%x 0x%x 0x%x  0x%x\n",
			*pDst, *(pDst+1), *(pDst+126),
			*(pDst+127), *(pDst+128));

		for (i = 0; i < 128; i++) {
			if (*(pDst+i) != 0) {
				m4u_err("(4) [Error] pDst check fail !!VA 0x%p: 0x%x\n",
					pDst+i*sizeof(unsigned char),
					*(pDst+i));
				break;
			}
		}
		if (i == 128)
			m4u_err("(4) m4u_display_fake_engine_test R/W 128 bytes PASS!!\n ");

		vfree(pSrc);
		vfree(pDst);

		m4u_monitor_stop(0);
#endif
		break;
	}

#ifdef M4U_TEE_SERVICE_ENABLE
	case 50:
	{
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		u32 sec_handle = 0;
		int ret = 0;
#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
		u32 refcount = 0;

		ret = trusted_mem_api_alloc(0, 0, 0x1000, &refcount,
					    &sec_handle, "m4u_ut", 0);
#endif
		if (ret == -ENOMEM) {
			m4u_err("%s[%d] UT FAIL: out of memory\n",
			       __func__, __LINE__);
			return ret;
		}
		if (sec_handle <= 0) {
			m4u_err("%s[%d] sec memory alloc error: handle %u\n",
			       __func__, __LINE__, sec_handle);
			return sec_handle;
		}

#elif defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
		u32 sec_handle = 0;
		int ret = 0;

		ret = KREE_AllocSecurechunkmemWithTag(0, &sec_handle,
						      0, 0x1000, "m4u_ut");
		if (ret != TZ_RESULT_SUCCESS) {
			m4u_err("KREE_Alloc failed, ret is 0x%x\n", ret);
			return ret;
		}
#endif
		m4u_sec_init();
	break;
	}
#if 0
	case 51:
	{
		struct m4u_port_config_struct port;

		memset(&port, 0, sizeof(struct m4u_port_config_struct));

		port.ePortID = M4U_PORT_HW_VDEC_PP_EXT;
		port.Virtuality = 1;
		m4u_err("(0) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port(&port);
		/* port.ePortID = M4U_PORT_MDP_WROT1;*/
		/*m4u_config_port(&port); */
		/* port.ePortID = M4U_PORT_IMGO;*/
		/*m4u_config_port(&port); */
		port.ePortID = M4U_PORT_VENC_RCPU;
		m4u_config_port(&port);
		/* port.ePortID = M4U_PORT_MJC_MV_RD;*/
		/*m4u_config_port(&port); */

		port.ePortID = M4U_PORT_HW_VDEC_PP_EXT;
		m4u_err("(1) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port_tee(&port);
		port.Security = 1;
		m4u_err("(2) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port_tee(&port);
	}
	break;
#endif
#endif
	default:
		m4u_err("%s error,val=%llu\n", __func__, val);
	}
#endif

	return 0;
}

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_fops,
	m4u_debug_get,
	m4u_debug_set,
	"%llu\n");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
DEFINE_PROC_ATTRIBUTE(m4u_proc_fops,
	m4u_debug_get,
	m4u_debug_set,
	"%llu\n");
#endif


#if (M4U_DVT != 0)
static void m4u_test_init(void)
{
}

static void m4u_test_start(void)
{
}

static void m4u_test_end(int invalid_tlb)
{
}
#endif

#if (M4U_DVT != 0)
static int __vCatchTranslationFault(struct m4u_domain *domain,
					unsigned int layer,
					unsigned int seed_mva)
{
	struct imu_pgd *pgd;
	struct imu_pte *pte;
	unsigned int backup;
	unsigned int *backup_ptr;
	int count;

	int pt_type = m4u_get_pt_type(domain, seed_mva);

	m4u_err("%s, layer = %d, seed_mva = 0x%x.\n",
		__func__, layer, seed_mva);

	if (seed_mva == 0) {
		m4u_err("seed_mva = 0!\n");
		return 0;
	}

	pgd = imu_pgd_offset(domain, seed_mva);
	if (layer == 0) {
		int i = 0;

		backup = imu_pgd_val(*pgd);
		backup_ptr = (unsigned int *)pgd;
		if (pt_type == MMU_PT_TYPE_SUPERSECTION) {
			for (i = 0; i < 16; i++)
				imu_pgd_val(*(pgd + i)) = 0x0;
		} else {
			imu_pgd_val(*pgd) = 0x0;
		}
	} else {
		int i = 0;

		pte = imu_pte_offset_map(pgd, seed_mva);
		backup = imu_pte_val(*pte);
		backup_ptr = (unsigned int *)pte;
		if (pt_type == MMU_PT_TYPE_LARGE_PAGE) {
			for (i = 0; i < 16; i++)
				imu_pte_val(*(pte + i)) = 0x0;
		} else {
			imu_pte_val(*pte) = 0x0;
		}
	}

	for (count = 0; count < 100; count++)
		m4u_err("test %d ......\n", count);

	/* restore */
	*backup_ptr = backup;

	return 0;
}

static int __vCatchInvalidPhyFault(struct m4u_domain *domain,
					int g4_mode, unsigned int seed_mva)
{
	struct imu_pgd *pgd;
	struct imu_pte *pte;
	unsigned int backup;
	unsigned int fault_pa;
	int count;

	if (seed_mva == 0) {
		m4u_err("seed_mva = 0 !!!!!!!!!!!\n");
		return 0;
	}

	pgd = imu_pgd_offset(domain, seed_mva);
#if (M4U_DVT == MMU_PT_TYPE_SMALL_PAGE || M4U_DVT == MMU_PT_TYPE_LARGE_PAGE)
	pte = imu_pte_offset_map(pgd, seed_mva);
	backup = imu_pte_val(*pte);
	if (!g4_mode) {
		imu_pte_val(*pte) = 0x2;
		fault_pa = 0;
	} else {
		imu_pte_val(*pte) = 0x10000002;
		fault_pa = 0x10000000;
	}
#else
	backup = imu_pgd_val(*pgd);
	if (!g4_mode) {
		imu_pgd_val(*pgd) = 0x2;
		fault_pa = 0;
	} else {
		imu_pgd_val(*pgd) = 0x10000002;
		fault_pa = 0x10000000;
	}
#endif
	m4u_err("fault_pa (%d): 0x%x\n", g4_mode, fault_pa);

	for (count = 0; count < 100; count++)
		m4u_err("test %d ......\n", count);


	/* restore */
#if (M4U_DVT == MMU_PT_TYPE_SMALL_PAGE || M4U_DVT == MMU_PT_TYPE_LARGE_PAGE)
	imu_pte_val(*pte) = backup;
#else
	imu_pgd_val(*pgd) = backup;
#endif
	return 0;
}

#endif

#if (M4U_DVT != 0)
static int m4u_test_set(void *data, u64 val)
{
	return 0;
}

static int m4u_test_get(void *data, u64 *val)
{
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_test_fops,
	m4u_test_get,
	m4u_test_set,
	"%llu\n");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
DEFINE_PROC_ATTRIBUTE(m4u_proc_test_fops,
	m4u_test_get,
	m4u_test_set,
	"%llu\n");
#endif
#endif

static int m4u_log_level_set(void *data, u64 val)
{
	return 0;
}

static int m4u_log_level_get(void *data, u64 *val)
{
	*val = gM4U_log_level | (gM4U_log_to_uart << 4);

	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_log_level_fops,
	m4u_log_level_get,
	m4u_log_level_set,
	"%llu\n");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
DEFINE_PROC_ATTRIBUTE(m4u_proc_log_level_fops,
	m4u_log_level_get,
	m4u_log_level_set,
	"%llu\n");
#endif

static int m4u_debug_freemva_set(void *data, u64 val)
{
	return 0;
}

static int m4u_debug_freemva_get(void *data, u64 *val)
{
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_freemva_fops,
	m4u_debug_freemva_get,
	m4u_debug_freemva_set,
	"%llu\n");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
DEFINE_PROC_ATTRIBUTE(m4u_proc_freemva_fops,
	m4u_debug_freemva_get,
	m4u_debug_freemva_set,
	"%llu\n");
#endif


int m4u_debug_port_show(struct seq_file *s, void *unused)
{
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_port_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_debug_port_fops = {
	.open = m4u_debug_port_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_port_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_proc_port_fops = {
	.open = m4u_proc_port_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_mva_show(struct seq_file *s, void *unused)
{
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_mva_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_debug_mva_fops = {
	.open = m4u_debug_mva_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_mva_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_proc_mva_fops = {
	.open = m4u_proc_mva_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_buf_show(struct seq_file *s, void *unused)
{
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_buf_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_debug_buf_fops = {
	.open = m4u_debug_buf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_buf_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_proc_buf_fops = {
	.open = m4u_proc_buf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_monitor_show(struct seq_file *s, void *unused)
{
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_monitor_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_debug_monitor_fops = {
	.open = m4u_debug_monitor_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_monitor_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_proc_monitor_fops = {
	.open = m4u_proc_monitor_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_register_show(struct seq_file *s, void *unused)
{
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_register_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_debug_register_fops = {
	.open = m4u_debug_register_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_register_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations m4u_proc_register_fops = {
	.open = m4u_proc_register_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_init(struct m4u_device *m4u_dev)
{
	return 0;
}
#else
int m4u_debug_init(struct m4u_device *m4u_dev)
{
	/* do nothing */
}
#endif
