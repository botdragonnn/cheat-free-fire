#include "Namegun.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

std::vector<Namegun::GunInfo> Namegun::GunData;

void Namegun::Init() {
    if (!GunData.empty()) return;

    GunData.resize(25000);

    // Especiais
    GunData[6016] = { "CHAMAR AIRDROP", "", true };
    GunData[10006] = { "DROP CAMINHÃO", "", true };
    GunData[-15524 + 25000] = { "Mini Drone de cura", "", true };
    GunData[-15521 + 25000] = { "Mini Drone de cura (1 USO)", "", true };
    GunData[21001] = { "Pistoleta de cura", "\ue066", true };
    GunData[21002] = { "M590", "\ue084", true };

    // Bot's
    GunData[0] = { "AK47", "\ue074" };

    // Fuzil 
    GunData[2] = { "M4A1", "\ue075" };
    GunData[80] = { "M4A1-I", "\ue075", false, true };
    GunData[81] = { "M4A1-II", "\ue075", false, true };
    GunData[82] = { "M4A1-III", "\ue075", false, true };
    GunData[6] = { "AK47", "\ue074" };
    GunData[11] = { "M14", "\ue079" };
    GunData[63] = { "M14-I", "\ue079" , false, true };
    GunData[126] = { "M14-II", "\ue079" , false, true };
    GunData[127] = { "M14-III", "\ue079" , false, true };
    GunData[12] = { "SCAR", "\ue078" };
    GunData[178] = { "SCAR-I", "\ue078" , false, true };
    GunData[179] = { "SCAR-II", "\ue078" , false, true };
    GunData[180] = { "SCAR-III", "\ue078" , false, true };
    GunData[14] = { "GROZA", "\ue07b" };
    GunData[70] = { "GROZA-X", "\ue07b", false, true };
    GunData[24] = { "FAMAS", "\ue07a" };
    GunData[67] = { "FAMAS-I", "\ue07a" , false, true };
    GunData[130] = { "FAMAS-II", "\ue07a" , false, true };
    GunData[131] = { "FAMAS-III", "\ue07a" , false, true };
    GunData[28] = { "XM8", "\ue077" };
    GunData[33] = { "AN94", "\ue07d" };
    GunData[39] = { "Plasma", "\ue07c" };
    GunData[46] = { "AUG", "\ue076" };
    GunData[193] = { "AUG-I", "\ue076" , false, true };
    GunData[194] = { "AUG-II", "\ue076" , false, true };
    GunData[195] = { "AUG-III", "\ue076" , false, true };
    GunData[47] = { "Parafal", "\ue07f" };
    GunData[57] = { "Atiradeira", "\ue07e" };
    GunData[73] = { "G36-ASSALTO", "\ue087" };
    GunData[74] = { "G36-ALCANCE", "\ue087" };

    // Fuzil De Atirador 
    GunData[18] = { "SKS", "\ue071" };
    GunData[26] = { "SVD", "\ue070" };
    GunData[72] = { "SVD-Y", "\ue070", false, true };
    GunData[48] = { "Carapina", "\ue073" };
    GunData[89] = { "AC80", "\ue072" };

    // Metralhadora
    GunData[19] = { "M249", "\ue06f" };
    GunData[71] = { "M249-X", "\ue06f" , false, true };
    GunData[30] = { "M60", "\ue06e" };
    GunData[61] = { "M60-I", "\ue06e" , false, true };
    GunData[122] = { "M60-II", "\ue06e" , false, true };
    GunData[123] = { "M60-III", "\ue06e" , false, true };
    GunData[54] = { "Kord", "" };

    // Submetralhadoras (SMG)
    GunData[7] = { "UMP", "\ue063" };
    GunData[8] = { "MP5", "\ue061" };
    GunData[60] = { "MP5-I", "\ue061" , false, true };
    GunData[120] = { "MP5-II", "\ue061" , false, true };
    GunData[121] = { "MP5-III", "\ue061" , false, true };
    GunData[13] = { "VSS", "\ue065" };
    GunData[62] = { "VSS-I", "\ue065" , false, true };
    GunData[124] = { "VSS-II", "\ue065" , false, true };
    GunData[125] = { "VSS-III", "\ue065" , false, true };
    GunData[15] = { "MP40", "\ue064" };
    GunData[32] = { "P90", "\ue069" };
    GunData[35] = { "CG15", "\ue067" };
    GunData[43] = { "Thompson", "\ue06b" };
    GunData[49] = { "Vector", "\ue06a" };
    GunData[69] = { "Double-Vector", "\ue088" };
    GunData[88] = { "MAC10", "\ue06d" };
    GunData[228] = { "MAC10-I", "\ue06d" , false, true };
    GunData[229] = { "MAC10-II", "\ue06d" , false, true };
    GunData[230] = { "MAC10-III", "\ue06d" , false, true };
    GunData[150] = { "Bis\xe3o", "\uE06C" };

    // Espingarda 
    GunData[5] = { "M1014", "\ue059" };
    GunData[184] = { "M1014-I", "\ue059" , false, true };
    GunData[185] = { "M1014-II", "\ue059" , false, true };
    GunData[186] = { "M1014-III", "\ue059" , false, true };
    GunData[29] = { "SPAS12", "\ue05a" };
    GunData[41] = { "M1887", "\ue05d" };
    GunData[119] = { "M1887-X", "\ue05d" , false, true };
    GunData[50] = { "MAG-7", "\ue05b" };
    GunData[86] = { "Carga Extra", "\ue05F" };
    GunData[181] = { "Trogon-Espingarda", "\ue05e" };
    GunData[182] = { "Trogon-Granada", "\ue05e" };

    // Fuzil De Precisão
    GunData[4] = { "AWM", "\ue04f" };
    GunData[65] = { "AWM-Y", "\ue04f" , false, true };
    GunData[21] = { "Kar98K", "\ue050" , false, true };
    GunData[64] = { "Kar98K-I", "\ue050" , false, true };
    GunData[128] = { "Kar98K-II", "\ue050", false, true };
    GunData[129] = { "Kar98K-III", "\ue050" , false, true };
    GunData[45] = { "M82B", "\ue053" };
    GunData[75] = { "M24", "\ue054" };
    GunData[78] = { "FP DE CURA", "" };
    GunData[197] = { "VSK94", "\ue056" };

    // Pistola
    GunData[3] = { "USP", "\ue04e" };
    GunData[56] = { "USP-2", "\ue060" , false, true };
    GunData[9] = { (const char*)u8"Águia do Deserto", "\ue051" };
    GunData[10] = { "G18", "\ue052" };
    GunData[20] = { "M1873", "\ue057" };
    GunData[25] = { "M500", "\ue058" };
    GunData[55] = { "M1917", "\ue05c" };
    GunData[58] = { "Mini Uzi", "\ue062" };
    GunData[93] = { "Pistola de Cura", "\ue066" };

    // Armas De Contato
    GunData[16] = { "Panela", "\ue04a" };
    GunData[17] = { "Machete", "\ue04b" };
    GunData[27] = { "Bast\xe3o", "\ue04c" };
    GunData[34] = { "Katana", "" };
    GunData[51] = { "Foice", "\ue04d" };
    GunData[53] = { "Faca FF", "\ue085" };

    // Punho
    GunData[1] = { "Punho", "\ue005" };

    // Granada
    GunData[601] = { "Granada", "\ue080" };
    GunData[603] = { "Granada de fuma\xE7""a", "\uE086" };
    GunData[1201] = { "Parede De Gel", "\ue081" };
    GunData[1204] = { "Parede de Gel", "\ue081" };
    GunData[602] = { "Granada De Luz", "\ue082" };
    GunData[608] = { "Bomba Congelante", "\ue083" };

    // Outros
    GunData[23] = { "M79", "" };
    GunData[36] = { "RGS-50", "" };
    GunData[196] = { "FGL-24", "" };

    // Outros 2
    GunData[100] = { "Lança-Chamas", "\ue068" };
    GunData[99] = { "Arma De Escudo", "" };
    GunData[617] = { "Quebra-Gel", "" };
    GunData[1401] = { "Minas Terrestres", "" };
    GunData[1006] = { "Mini Drone", "" };
    GunData[1015] = { "Caixote de Suprimentos", "" };
}



std::string Namegun::GetGunName(short gunId) {
    try {
        int adjustedId = gunId < 0 ? gunId + 25000 : gunId;

        if (adjustedId >= 0 && adjustedId < GunData.size() &&
            (!GunData[adjustedId].name.empty() || GunData[adjustedId].isSpecial)) {
            return GunData[adjustedId].name;
        }

        std::cout << "[UNKNOWN WEAPON] ID: " << gunId << " precisa ser adicionado!" << std::endl;
        return "ID: " + std::to_string(gunId);
    }
    catch (const std::exception& ex) {
        std::cout << "Erro ao obter nome da arma: " << ex.what() << std::endl;
        return "Unknown";
    }
}

std::string Namegun::GetGunIcon(short gunId) {
    try {
        int adjustedId = gunId < 0 ? gunId + 25000 : gunId;

        if (adjustedId >= 0 && adjustedId < GunData.size()) {
            if (!GunData[adjustedId].icon.empty()) {
                if (GunData[adjustedId].hasLevels) {
                    std::string baseName = GetBaseName(GunData[adjustedId].name);
                    std::string suffix = GunData[adjustedId].name.substr(baseName.length());
                    return GunData[adjustedId].icon + suffix;
                }
                return GunData[adjustedId].icon;
            }
            return GunData[adjustedId].name;
        }
        return "";
    }
    catch (const std::exception& ex) {
        std::cout << "Erro ao obter ícone da arma: " << ex.what() << std::endl;
        return "Unknown";
    }
}

std::string Namegun::GetBaseName(const std::string& fullName) {
    size_t dashPos = fullName.find_last_of('-');
    if (dashPos != std::string::npos) {
        std::string suffix = fullName.substr(dashPos + 1);
        if (suffix == "I" || suffix == "II" || suffix == "III" ||
            suffix == "X" || suffix == "Y" || suffix == "2") {
            return fullName.substr(0, dashPos);
        }
    }
    return fullName;
}

bool Namegun::HasIcon(short gunId) {
    int adjustedId = gunId < 0 ? gunId + 25000 : gunId;
    return (adjustedId >= 0 && adjustedId < GunData.size() &&
        !GunData[adjustedId].icon.empty());
}