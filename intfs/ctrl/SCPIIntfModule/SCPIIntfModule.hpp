#ifndef SCPIINTMODULE_H_
#define SCPIINTMODULE_H_
extern "C" {
    #include "scpi/scpi.h"
}

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>


struct SCPICtx {
    class SCPIIntfModule* self;
    void*          ctx;
};

class SCPIIntfModule {
public:
    SCPIIntfModule(const scpi_command_t * commands, void * ctx);

    ~SCPIIntfModule() =  default;

    void Process(const char *cmd);
    const char * Response() const {return _response_buffer;};

private:
    static size_t SCPI_Write(scpi_t *ctx, const char *data, size_t len);
    static int SCPI_Error(scpi_t *ctx, int_fast16_t err);
    static scpi_result_t SCPI_Flush(scpi_t * ctx);

    SCPICtx _holder{};

    scpi_t _scpi;
    char _response_buffer[32];
    char _input_buffer[32];
    scpi_error_t _error_queue[8];

    const scpi_command_t *_commands;
};
#endif
