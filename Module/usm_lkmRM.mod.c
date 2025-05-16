#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x78e8bf4, "module_layout" },
	{ 0xf50816fe, "dma_free_attrs" },
	{ 0x956198bb, "dma_alloc_attrs" },
	{ 0x8adfea3f, "proc_remove" },
	{ 0x1e72d524, "misc_deregister" },
	{ 0xbec2ee7b, "proc_create" },
	{ 0xa210e2a0, "misc_register" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0xabc4cd40, "usmPagesCount" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x8d6c053, "usmPagesLock" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xa9a801ba, "usm_poll_ufd" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0xdccbe141, "handle_page" },
	{ 0x8c2571eb, "remap_pfn_range" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x25f6daa, "wake_up_process" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x77006f26, "handle_usm" },
	{ 0x92997ed8, "_printk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "8AE4E0A4227B5B8CE4CA03F");
