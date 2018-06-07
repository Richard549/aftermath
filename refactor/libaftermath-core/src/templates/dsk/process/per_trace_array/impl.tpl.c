{%- set pargs = process.args -%}
{%- set memtype = mem.find(pargs.mem_struct_name) -%}

{% include "fnproto.tpl.h" %}
{
	struct {{pargs.trace_array_struct_name}}* a = &ctx->trace->{{pargs.trace_array_field}};
	{{memtype.c_type}}* mem;

	{% if pargs.dsk_struct_sort_field %}
	if(!(mem = {{pargs.trace_array_struct_name}}_reserve_sorted(a, f->{{pargs.dsk_struct_sort_field}}))) {
		AM_IOERR_GOTO_NA(ctx, out_err, AM_IOERR_ALLOC,
				 "Could not allocate space for a {{memtype.entity}}.");
	}
	{% else %}
	if({{pargs.trace_array_struct_name}}_reserve_end(a)) {
		AM_IOERR_GOTO_NA(ctx, out_err, AM_IOERR_ALLOC,
				 "Could not allocate space for a {{memtype.entity}}.");
	}

	mem = &a->elements[a->num_elements-1];
	{% endif %}

	if({{pargs.dsk_to_mem_function}}(ctx, f, mem)) {
		AM_IOERR_GOTO_NA(ctx, out_err_assign, AM_IOERR_CONVERT,
				 "Could not assign values from an on-disk {{memtype.entity}}.");
	}

	return 0;

out_err_assign:
	{{pargs.trace_array_struct_name}}_removep(a, mem);
out_err:
	return 1;
}