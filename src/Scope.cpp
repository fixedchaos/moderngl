#include "Types.hpp"

#include "InlineMethods.hpp"

PyObject * MGLContext_scope(MGLContext * self, PyObject * args) {
	MGLFramebuffer * framebuffer;
	PyObject * enable_flags;
	PyObject * textures;
	PyObject * uniform_buffers;
	PyObject * shader_storage_buffers;

	int args_ok = PyArg_ParseTuple(
		args,
		"O!OOOO",
		MGLFramebuffer_type,
		&framebuffer,
		&enable_flags,
		&textures,
		&uniform_buffers,
		&shader_storage_buffers
	);

	if (!args_ok) {
		return 0;
	}

	int flags = MGL_INVALID;
	if (enable_flags != Py_None) {
		flags = PyLong_AsLong(enable_flags);
		if (PyErr_Occurred()) {
			MGLError_Set("invalid enable_flags");
			return 0;
		}
	}

	MGLScope * scope = PyObject_New(MGLScope, MGLScope_type);

	Py_INCREF(self);
	scope->context = self;

	scope->enable_flags = flags;

	Py_INCREF(framebuffer);
	scope->framebuffer = framebuffer;

	Py_INCREF(self->bound_framebuffer);
	scope->old_framebuffer = self->bound_framebuffer;

	int num_textures = (int)PyTuple_Size(textures);
	int num_uniform_buffers = (int)PyTuple_Size(uniform_buffers);
	int num_shader_storage_buffers = (int)PyTuple_Size(shader_storage_buffers);

	scope->num_textures = num_textures;
	scope->textures = new int[scope->num_textures * 3];
	scope->num_buffers = num_uniform_buffers + num_shader_storage_buffers;
	scope->buffers = new int[scope->num_buffers * 3];

	for (int i = 0; i < num_textures; ++i) {
		PyObject * tup = PyTuple_GET_ITEM(textures, i);
		PyObject * item = PyTuple_GET_ITEM(tup, 0);

		int texture_type;
		int texture_obj;

		if (Py_TYPE(item) == MGLTexture_type) {
			MGLTexture * texture = (MGLTexture *)item;
			texture_type = texture->samples ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
			texture_obj = texture->texture_obj;
		} else if (Py_TYPE(item) == MGLTexture3D_type) {
			MGLTexture3D * texture = (MGLTexture3D *)item;
			texture_type = GL_TEXTURE_3D;
			texture_obj = texture->texture_obj;
		} else if (Py_TYPE(item) == MGLTextureCube_type) {
			MGLTextureCube * texture = (MGLTextureCube *)item;
			texture_type = GL_TEXTURE_CUBE_MAP;
			texture_obj = texture->texture_obj;
		} else {
			MGLError_Set("invalid texture");
			return 0;
		}

		int binding = PyLong_AsLong(PyTuple_GET_ITEM(tup, 1));
		scope->textures[i * 3 + 0] = GL_TEXTURE0 + binding;
		scope->textures[i * 3 + 1] = texture_type;
		scope->textures[i * 3 + 2] = texture_obj;
	}

	for (int i = 0; i < num_uniform_buffers; ++i) {
		PyObject * tup = PyTuple_GET_ITEM(uniform_buffers, i);
		MGLBuffer * buffer = (MGLBuffer *)PyTuple_GET_ITEM(tup, 0);

		if (Py_TYPE(buffer) == MGLBuffer_type) {
			int binding = PyLong_AsLong(PyTuple_GET_ITEM(tup, 1));
			scope->buffers[i * 3 + 0] = GL_UNIFORM_BUFFER;
			scope->buffers[i * 3 + 1] = buffer->buffer_obj;
			scope->buffers[i * 3 + 2] = binding;
		} else {
			MGLError_Set("invalid buffer");
			return 0;
		}
	}

	int base = num_uniform_buffers * 3;

	for (int i = 0; i < num_shader_storage_buffers; ++i) {
		PyObject * tup = PyTuple_GET_ITEM(shader_storage_buffers, i);
		MGLBuffer * buffer = (MGLBuffer *)PyTuple_GET_ITEM(tup, 0);

		if (Py_TYPE(buffer) == MGLBuffer_type) {
			int binding = PyLong_AsLong(PyTuple_GET_ITEM(tup, 1));
			scope->buffers[base + i * 3 + 0] = GL_SHADER_STORAGE_BUFFER;
			scope->buffers[base + i * 3 + 1] = buffer->buffer_obj;
			scope->buffers[base + i * 3 + 2] = binding;
		} else {
			MGLError_Set("invalid buffer");
			return 0;
		}
	}

	return (PyObject *)scope;
}

extern PyObject * MGLFramebuffer_use(MGLFramebuffer * self);

PyObject * MGLScope_begin(MGLScope * self, PyObject * args) {
	int args_ok = PyArg_ParseTuple(
		args,
		""
	);

	if (!args_ok) {
		return 0;
	}

	const GLMethods & gl = self->context->gl;
	const int & flags = self->enable_flags;

	self->old_enable_flags = self->context->enable_flags;
	self->context->enable_flags = self->enable_flags;

	MGLFramebuffer_use(self->framebuffer);

	for (int i = 0; i < self->num_textures; ++i) {
		gl.ActiveTexture(self->textures[i * 3]);
		gl.BindTexture(self->textures[i * 3 + 1], self->textures[i * 3 + 2]);
	}

	for (int i = 0; i < self->num_buffers; ++i) {
		gl.BindBufferBase(self->buffers[i * 3], self->buffers[i * 3 + 1], self->buffers[i * 3 + 2]);
	}

	if (flags & MGL_BLEND) {
		gl.Enable(GL_BLEND);
	} else {
		gl.Disable(GL_BLEND);
	}

	if (flags & MGL_DEPTH_TEST) {
		gl.Enable(GL_DEPTH_TEST);
	} else {
		gl.Disable(GL_DEPTH_TEST);
	}

	if (flags & MGL_CULL_FACE) {
		gl.Enable(GL_CULL_FACE);
	} else {
		gl.Disable(GL_CULL_FACE);
	}

	if (flags & MGL_RASTERIZER_DISCARD) {
		gl.Enable(GL_RASTERIZER_DISCARD);
	} else {
		gl.Disable(GL_RASTERIZER_DISCARD);
	}

	Py_RETURN_NONE;
}

PyObject * MGLScope_end(MGLScope * self, PyObject * args) {
	int args_ok = PyArg_ParseTuple(
		args,
		""
	);

	if (!args_ok) {
		return 0;
	}

	const GLMethods & gl = self->context->gl;
	const int & flags = self->old_enable_flags;

	self->context->enable_flags = self->old_enable_flags;

	MGLFramebuffer_use(self->old_framebuffer);

	if (flags & MGL_BLEND) {
		gl.Enable(GL_BLEND);
	} else {
		gl.Disable(GL_BLEND);
	}

	if (flags & MGL_DEPTH_TEST) {
		gl.Enable(GL_DEPTH_TEST);
	} else {
		gl.Disable(GL_DEPTH_TEST);
	}

	if (flags & MGL_CULL_FACE) {
		gl.Enable(GL_CULL_FACE);
	} else {
		gl.Disable(GL_CULL_FACE);
	}

	if (flags & MGL_RASTERIZER_DISCARD) {
		gl.Enable(GL_RASTERIZER_DISCARD);
	} else {
		gl.Disable(GL_RASTERIZER_DISCARD);
	}

	Py_RETURN_NONE;
}

PyMethodDef MGLScope_tp_methods[] = {
	{"begin", (PyCFunction)MGLScope_begin, METH_VARARGS, 0},
	{"end", (PyCFunction)MGLScope_end, METH_VARARGS, 0},
	{0},
};

PyTypeObject * MGLScope_type;

PyType_Slot MGLScope_slots[] = {
	{Py_tp_methods, MGLScope_tp_methods},
	{0},
};

PyType_Spec MGLScope_spec = {"MGLScope", sizeof(MGLScope), 0, Py_TPFLAGS_DEFAULT, MGLScope_slots};
