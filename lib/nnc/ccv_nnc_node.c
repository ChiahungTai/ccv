#include "ccv_nnc.h"

typedef void(*ccv_nnc_init_f)(ccv_nnc_node_api_t node_api[]);

typedef struct {
	int backend;
	ccv_nnc_init_f init;
} ccv_nnc_init_t;

#define CCV_NNC_INIT_DECL(init_func) extern void (init_func)(ccv_nnc_node_api_t node_api[])
#define CCV_NNC_INIT_MAP_BEGIN() static ccv_nnc_init_t init_map[] = {
#define CCV_NNC_INIT_MAP(name, init_func) { .backend = name, .init = init_func, },
#define CCV_NNC_INIT_MAP_END() };

#define CCV_NNC_INIT_EXEC(name, init_func) do { \
		(init_func)(node_api_decls[name]); \
	} while (0)

void ccv_nnc_gpu_ref_init(ccv_nnc_node_api_t node_api[])
{
}

void ccv_nnc_gpu_cudnn_init(ccv_nnc_node_api_t node_api[])
{
}

// I should be able to automatically extract code below from source code.
enum {
	CCV_NNC_BACKEND_CPU_REF = 0,
	CCV_NNC_BACKEND_GPU_REF,
	CCV_NNC_BACKEND_GPU_CUDNN,
	CCV_NNC_BACKEND_COUNT,
};

CCV_NNC_INIT_DECL(ccv_nnc_cpu_ref_init);
CCV_NNC_INIT_DECL(ccv_nnc_gpu_ref_init);
CCV_NNC_INIT_DECL(ccv_nnc_gpu_cudnn_init);

CCV_NNC_INIT_MAP_BEGIN()
CCV_NNC_INIT_MAP(CCV_NNC_BACKEND_CPU_REF, ccv_nnc_cpu_ref_init)
CCV_NNC_INIT_MAP(CCV_NNC_BACKEND_GPU_REF, ccv_nnc_gpu_ref_init)
CCV_NNC_INIT_MAP(CCV_NNC_BACKEND_GPU_CUDNN, ccv_nnc_gpu_cudnn_init)
CCV_NNC_INIT_MAP_END()
// Above should be automatic generated.

static ccv_nnc_node_api_t node_api_decls[CCV_NNC_BACKEND_COUNT][CCV_NNC_COMPUTE_COUNT];

void ccv_nnc_init(void)
{
	int i;
	int count = sizeof(init_map) / sizeof(ccv_nnc_init_t);
	// Init dynamic dispatch table.
	for (i = 0; i < count; i++)
		init_map[i].init(node_api_decls[init_map[i].backend]);
}

ccv_nnc_net_node_t ccv_nnc_net_node(const int compute, const ccv_nnc_net_node_param_t params, const int flags)
{
	ccv_nnc_net_node_t node;
	node.info = params;
	// TODO: auto-find a workable implementation.
	node.backend = CCV_NNC_BACKEND_CPU_REF;
	node.compute = compute;
	return node;
}

int ccv_nnc_net_hint_verify(const ccv_nnc_net_hint_t hint, const ccv_nnc_net_node_param_t node, const ccv_nnc_tensor_param_t a, const ccv_nnc_tensor_param_t b)
{
	int i;
	// 0-dim is reserved for channels
	for (i = 1; i < CCV_NNC_MAX_DIM + 1; i++)
	{
		if ((hint.border.begin[i] + hint.border.end[i] + a.dim[i] - node.size.dim[i]) % hint.stride.dim[i] != 0)
			return -1;
		int expected = (hint.border.begin[i] + hint.border.end[i] + a.dim[i] - node.size.dim[i]) / hint.stride.dim[i] + 1;
		if (expected != b.dim[i])
			return -1;
	}
	return 0;
}

ccv_nnc_net_hint_t ccv_nnc_net_hint_guess(const ccv_nnc_net_node_param_t node, const ccv_nnc_tensor_param_t* inputs, const int input_size, const ccv_nnc_tensor_param_t* outputs, const int output_size)
{
	ccv_nnc_net_hint_t guess;
	guess.stride.dim[0] = 0;
	guess.border.begin[0] = 0;
	guess.border.end[0] = 0;
	assert(input_size == 1);
	assert(output_size == 1);
	const ccv_nnc_tensor_param_t a = inputs[0];
	const ccv_nnc_tensor_param_t b = outputs[0];
	int i;
	// 0-dim is reserved for channels
	for (i = 1; i < CCV_NNC_MAX_DIM + 1; i++)
	{
		// This is guessed by having a stride that will approximately match the scale.
		int stride = (a.dim[i] + b.dim[i] / 2) / b.dim[i];
		guess.stride.dim[i] = stride;
		int border = (b.dim[i] - 1) * stride - a.dim[i] + node.size.dim[i];
		guess.border.begin[i] = border / 2;
		guess.border.end[i] = border - guess.border.begin[i];
	}
	return guess;
}

void ccv_nnc_net_node_exec(const ccv_nnc_net_node_t node, const ccv_nnc_net_hint_t hint, const int flags, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size)
{
	assert(node.backend < CCV_NNC_BACKEND_COUNT);
	assert(node.compute < CCV_NNC_COMPUTE_COUNT);
	ccv_nnc_node_api_t api_decl = node_api_decls[node.backend][node.compute];
	int i;
	for (i = 0; i < input_size; i++)
	{
		assert(api_decl.tensor_formats & inputs[i]->info.format);
	}
	for (i = 0; i < output_size; i++)
	{
		assert(api_decl.tensor_formats & outputs[i]->info.format);
	}
	api_decl.exec(node, hint, flags, inputs, input_size, outputs, output_size);
}
