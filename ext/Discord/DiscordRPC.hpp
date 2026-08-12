#pragma once
#include <string>

namespace DiscordRPC
{
    void Tick(bool enabled);

    void Shutdown();

    struct AvatarData
    {
        int width = 0;
        int height = 0;
        const unsigned char* pixels = nullptr;
    };

    bool GetAvatarData(AvatarData& out);

    // Nome do Discord do usuario logado (preenchido via RPC). Retorna false
    // se ainda nao conectou (ex: Discord desktop fechado).
    bool GetUsername(std::string& out);
}
