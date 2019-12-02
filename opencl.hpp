#ifndef OPENCL_HPP_INCLUDED
#define OPENCL_HPP_INCLUDED

#include <vector>
#include <map>
#include <string>
#include <cl/cl.h>
#include <memory>

namespace cl
{
    template<typename T, cl_int(*U)(T), cl_int(*V)(T), typename derived>
    struct base
    {
        T data;

        base()
        {
            data = nullptr;
        }

        base(const derived& other)
        {
            data = other.data;

            if(data)
            {
                U(data);
            }
        }

        derived& operator=(const derived& other)
        {
            if(this == &other)
                return *this;

            if(data)
            {
                V(data);
            }

            data = other.data;

            if(data)
            {
                U(data);
            }

            return *this;
        }

        base(derived&& other)
        {
            data = other.data;
            other.data = nullptr;
        }

        derived& operator=(derived&& other)
        {
            if(data)
            {
                V(data);
            }

            data = other.data;
            other.data = nullptr;

            return *this;
        }

        ~base()
        {
            if(data)
            {
                V(data);
            }
        }
    };

    struct arg_info
    {
        void* ptr = nullptr;
        int64_t size = 0;
    };

    struct args
    {
        std::vector<arg_info> arg_list;

        template<typename T>
        inline
        void push_back(T& val)
        {
            arg_info inf;
            inf.ptr = &val;
            inf.size = sizeof(T);

            arg_list.push_back(inf);
        }
    };


    struct event
    {
        base<cl_event, clRetainEvent, clReleaseEvent, event> native_event;
    };

    struct program;

    struct kernel
    {
        base<cl_kernel, clRetainKernel, clReleaseKernel, kernel> native_kernel;

        kernel();
        kernel(program& p, const std::string& name);
        kernel(cl_kernel k); ///non retaining

        std::string name;
    };

    struct program;

    struct context
    {
        std::vector<program> programs;
        std::shared_ptr<std::map<std::string, kernel>> kernels;
        cl_device_id selected_device;

        base<cl_context, clRetainContext, clReleaseContext, context> native_context;

        context();
        void register_program(program& p);
    };

    struct program
    {
        base<cl_program, clRetainProgram, clReleaseProgram, program> native_program;

        program(context& ctx, const std::string& data, bool is_file = true);
        void build(context& ctx, const std::string& options);
    };

    struct command_queue;

    struct mem_object
    {
        base<cl_mem, clRetainMemObject, clReleaseMemObject, mem_object> native_mem_object;
    };

    struct buffer : mem_object
    {
        base<cl_context, clRetainContext, clReleaseContext, context> native_context;
        int64_t alloc_size = 0;

        buffer(cl::context& ctx);

        void alloc(int64_t bytes);
        void write(command_queue& write_on, const char* ptr, int64_t bytes);

        template<typename T>
        void write(command_queue& write_on, const std::vector<T>& data)
        {
            if(data.size() == 0)
                return;

            write(write_on, (const char*)&data[0], data.size() * sizeof(T));
        }

        void read(command_queue& read_on, char* ptr, int64_t bytes);

        template<typename T>
        std::vector<T> read(command_queue& read_on)
        {
            std::vector<T> ret;

            if(alloc_size == 0)
                return ret;

            ret.resize(alloc_size / sizeof(T));

            read(read_on, &ret[0], alloc_size);
        }
    };

    struct command_queue
    {
        base<cl_command_queue, clRetainCommandQueue, clReleaseCommandQueue, command_queue> native_command_queue;
        base<cl_context, clRetainContext, clReleaseContext, context> native_context;
        std::shared_ptr<std::map<std::string, kernel>> kernels;

        command_queue(context& ctx, cl_command_queue_properties props = 0);

        void exec(const std::string& kname, args& pack, const std::vector<int>& global_ws, const std::vector<int>& local_ws);
    };

    //cl_event exec_1d(cl_command_queue cqueue, cl_kernel kernel, const std::vector<cl_mem>& args, const std::vector<size_t>& global_ws, const std::vector<size_t>& local_ws, const std::vector<cl_event>& waitlist);
}

template<>
inline
void cl::args::push_back<cl::mem_object>(cl::mem_object& val)
{
    cl::arg_info inf;
    inf.ptr = &val.native_mem_object.data;
    inf.size = sizeof(cl_mem);

    arg_list.push_back(inf);
}

#endif // OPENCL_HPP_INCLUDED