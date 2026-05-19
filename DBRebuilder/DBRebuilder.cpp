#include <shellapi.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <Voltek.RelocationDatabase.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>

#include "source_file.h"


#include <Zydis/DecoderTypes.h>

namespace voltek
{
    struct reldb_signature
    {
        uint32_t rva;
        std::string pattern;
    };
}

class reldb_protect
{
public:
    reldb_protect(voltek::reldb_stream* _s) : s(_s) {}
    ~reldb_protect() { if (s) voltek::reldb_release_db(s); }
private:
    voltek::reldb_stream* s;
};

void pause()
{
    char ch;
    std::cin >> ch;
}

void action_rebase(const char* fname_exe, const char* fname_database, bool save_database)
{
    auto Attr = GetFileAttributes(fname_exe);
    if ((Attr == INVALID_FILE_ATTRIBUTES) || ((Attr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY))
        return;

    source_file file_exe(fname_exe);

    auto temp = const_cast<char*>(fname_exe);
    PathStripPath(temp);
    std::cout << "File: " << temp << std::endl;

    uint32_t start, end;
    file_exe.get_code_range(&start, &end);
    printf("Range .text [ %08X in %08X ]\n", start, end);

    voltek::reldb_stream* database = nullptr;
    auto err = voltek::reldb_open_db(&database, fname_database);
    if (err)
    {
        std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
        return;
    }

    reldb_protect scope(database);

    auto total_patch = voltek::reldb_count_patches(database);
    std::cout << "Total patches: " << total_patch << std::endl;

    constexpr static int BUFSIZE = 60;

    long total_process = 0, total_failed = 0;
    char szBuf[BUFSIZE];
    voltek::reldb_patch* patch = nullptr;
    voltek::reldb_signature* signature = nullptr;

    auto DeleteDirectoryEx = [](LPCSTR wzDirectory)
    {
        CHAR szDir[MAX_PATH + 1];
        SHFILEOPSTRUCT fos = { 0 };

        StringCchCopy(szDir, MAX_PATH, wzDirectory);
        int len = lstrlen(szDir);
        szDir[len + 1] = 0;

        fos.wFunc = FO_DELETE;
        fos.pFrom = szDir;
        fos.fFlags = FOF_NO_UI;
        SHFileOperation(&fos);
    };

    DeleteDirectoryEx("signs");
    CreateDirectoryA("signs", NULL);

    for (long i = 0; i < total_patch; i++)
    {
        err = voltek::reldb_get_patch_by_id(database, &patch, (uint32_t)i);
        if (err)
        {
            std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
            return;
        }

        err = voltek::reldb_get_name_patch(patch, szBuf, BUFSIZE);
        if (err)
        {
            std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
            return;
        }

        printf("\rProgress: %59.59s | version %i ( %03i / %03i )", szBuf, voltek::reldb_get_version_patch(patch), i + 1, total_patch);
        std::cout.flush();

        std::vector<std::pair<uint32_t, std::string>> signs;

        auto total_signs = voltek::reldb_count_signatures_in_patch(patch);
        for (long j = 0; j < total_signs; j++)
        {
            total_process++;

            err = voltek::reldb_get_signature_patch(patch, &signature, (uint32_t)j);
            if (err)
            {
                std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
                return;
            }

            std::string mask;
            auto rva = voltek::reldb_get_rva_from_signature(signature);

            if (!rva || 
                !file_exe.is_rva_in_code_range(rva) || 
                !file_exe.get_mask_by_raw64_ex(rva, mask) ||
                (file_exe.find_patterns(mask).size() != 1))
            {
                total_failed++;
                signs.push_back(std::make_pair(rva, ""));

                continue;
            }

            signs.push_back(std::make_pair(rva, mask));
        }

        err = voltek::reldb_clear_signatures_in_patch(patch);
        if (err)
        {
            std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
            return;
        }

        for (auto it = signs.begin(); it != signs.end(); it++)
        {
            err = voltek::reldb_add_signature_to_patch(patch, it->first, it->second.c_str());
            if (err)
            {
                std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
                return;
            }
        }

        auto gen_fname = [](const char* fname_exe) -> std::string
        {
            std::string p = "signs/";
            std::string fn = fname_exe;
            fn.erase(std::remove_if(fn.begin(), fn.end(), isspace), fn.end());
            p.append(fn).append(".relb");
            return p;
        };

        voltek::reldb_save_dev_file_patch(database, patch, gen_fname(szBuf).c_str());

        Sleep(10);
    }

    if (save_database)
    {
        std::cout << std::endl << "Save database..." << std::endl;
        voltek::reldb_save_db(database);
    }
    else
        std::cout << std::endl;

    printf("Total signature [ success %u / fail %u ] (%u%%)\n", total_process, total_failed, ((total_failed * 100) / total_process));
}

void action_base_transform(const char* fname_exe, const char* fname_database, const char* nfname_exe)
{
    auto Attr = GetFileAttributes(fname_exe);
    if ((Attr == INVALID_FILE_ATTRIBUTES) || ((Attr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY))
        return;

    source_file old_file_exe(fname_exe);

    Attr = GetFileAttributes(nfname_exe);
    if ((Attr == INVALID_FILE_ATTRIBUTES) || ((Attr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY))
        return;

    source_file new_file_exe(nfname_exe);

    auto temp = const_cast<char*>(fname_exe);
    PathStripPath(temp);
    std::cout << "Source file: " << temp << std::endl;

    uint32_t start, end;
    old_file_exe.get_code_range(&start, &end);
    printf("Range .text [ %08X in %08X ]\n", start, end);

    temp = const_cast<char*>(nfname_exe);
    PathStripPath(temp);
    std::cout << "Dest file: " << temp << std::endl;

    new_file_exe.get_code_range(&start, &end);
    printf("Range .text [ %08X in %08X ]\n", start, end);

    voltek::reldb_stream* database = nullptr;
    auto err = voltek::reldb_open_db(&database, fname_database);
    if (err)
    {
        std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
        return;
    }

    reldb_protect scope(database);

    auto total_patch = voltek::reldb_count_patches(database);
    std::cout << "Total patches: " << total_patch << std::endl;

    constexpr static int BUFSIZE = 60;

    long total_process = 0, total_failed = 0;
    char szBuf[BUFSIZE];
    voltek::reldb_patch* patch = nullptr;
    voltek::reldb_signature* signature = nullptr;

    auto DeleteDirectoryEx = [](LPCSTR wzDirectory)
    {
        CHAR szDir[MAX_PATH + 1];
        SHFILEOPSTRUCT fos = { 0 };

        StringCchCopy(szDir, MAX_PATH, wzDirectory);
        int len = lstrlen(szDir);
        szDir[len + 1] = 0;

        fos.wFunc = FO_DELETE;
        fos.pFrom = szDir;
        fos.fFlags = FOF_NO_UI;
        SHFileOperation(&fos);
    };

    DeleteDirectoryEx("signs");
    CreateDirectoryA("signs", NULL);

    for (long i = 0; i < total_patch; i++)
    {
        err = voltek::reldb_get_patch_by_id(database, &patch, (uint32_t)i);
        if (err)
        {
            std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
            return;
        }

        err = voltek::reldb_get_name_patch(patch, szBuf, BUFSIZE);
        if (err)
        {
            std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
            return;
        }

        auto total_signs = voltek::reldb_count_signatures_in_patch(patch);

        std::cout << std::format("Patch: \"{}\" ({})", szBuf, total_signs) << std::endl;
        //printf("\rProgress: %59.59s | version %i ( %03i / %03i )", szBuf, voltek::reldb_get_version_patch(patch), i + 1, total_patch);
        std::cout.flush();

        std::vector<std::pair<uint32_t, std::string>> signs;
        for (long j = 0; j < total_signs; j++)
        {
            total_process++;

            err = voltek::reldb_get_signature_patch(patch, &signature, (uint32_t)j);
            if (err)
            {
                std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
                return;
            }

            std::string mask;
            auto rva = voltek::reldb_get_rva_from_signature(signature);

            if (!rva || 
                !old_file_exe.is_rva_in_code_range(rva))
            {
                total_failed++;
                signs.push_back(std::make_pair(rva, ""));

                continue;
            }

            if (old_file_exe.can_mask_by_raw64(signature->rva))
                old_file_exe.get_mask_by_raw64(signature->rva, mask);
            else
            {
                total_failed++;
                signs.push_back(std::make_pair(rva, ""));

                continue;
            }

            auto old_find_rva = old_file_exe.find_patterns(mask);
            auto new_find_rva = new_file_exe.find_patterns(mask);

            if (old_find_rva.size() == new_find_rva.size())
            {
                bool failed = true;
                for (size_t ids = 0; ids < old_find_rva.size(); ids++)
                {
                    if (old_find_rva[ids] == rva)
                    {
                        signs.push_back(std::make_pair(new_find_rva[ids], mask));
                        failed = false;
                        break;
                    }
                }

                if (failed)
                {
                    total_failed++;
                    signs.push_back(std::make_pair(0, ""));

                    continue;
                }
            }

           /* auto len = voltek::reldb_get_pattern_length_from_signature(signature);
            if (len > 0)
            {
                mask.resize(len);
                voltek::reldb_get_pattern_from_signature(signature, mask.data(), len);
            }
            else
            {
                if (old_file_exe.can_mask_by_raw64(signature->rva))
                {
                    old_file_exe.get_mask_by_raw64(signature->rva, mask);
                    signs.push_back(std::make_pair(rva, mask));

                    continue;
                }
                else
                {
                    total_failed++;
                    signs.push_back(std::make_pair(rva, ""));

                    continue;
                }
            }*/

           /* DWORD mode_sign = 0;

            if (mask.length() > 0)
            {
                if (mask[0] == '!')
                {
                    std::replace(mask.begin(), mask.end(), '_', ' ');
                    mode_sign = MAKELONG(1, strtol(mask.data() + 1, nullptr, 10));
                    mask.erase(0, mask.find_last_of(' ') + 1);
                }
            }
            else
            {
                total_failed++;
                signs.push_back(std::make_pair(rva, ""));

                continue;
            }

            switch (LOWORD(mode_sign))
            {
            case 1:
            {
                auto old_find_rva = old_file_exe.find_patterns(mask);
                if (old_find_rva.size() <= HIWORD(mode_sign))
                {
                    total_failed++;
                    signs.push_back(std::make_pair(0, ""));

                    continue;
                }

                ptrdiff_t distance = (ptrdiff_t)rva - old_find_rva[HIWORD(mode_sign)];

                auto new_find_rva = new_file_exe.find_patterns(mask);
                if ((old_find_rva.size() != old_find_rva.size()) || (new_find_rva.size() <= HIWORD(mode_sign)))
                {
                    total_failed++;
                    signs.push_back(std::make_pair(0, ""));

                    continue;
                }

                voltek::reldb_get_pattern_from_signature(signature, mask.data(), len);
                signs.push_back(std::make_pair((uint32_t)(((ptrdiff_t)new_find_rva[HIWORD(mode_sign)]) + distance), mask));
            }
            break;
            default:
            {
                auto old_find_rva = old_file_exe.find_pattern(mask);
                ptrdiff_t distance = (ptrdiff_t)rva - old_find_rva;

                auto new_find_rva = new_file_exe.find_patterns(mask);
                if (new_find_rva.size() != 1)
                {
                    total_failed++;
                    signs.push_back(std::make_pair(0, ""));

                    continue;
                }

                signs.push_back(std::make_pair((uint32_t)(((ptrdiff_t)new_find_rva[0]) + distance), mask));
            }
            break;
            }*/
        }

        err = voltek::reldb_clear_signatures_in_patch(patch);
        if (err)
        {
            std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
            return;
        }

        for (auto it = signs.begin(); it != signs.end(); it++)
        {
            err = voltek::reldb_add_signature_to_patch(patch, it->first, it->second.c_str(), false);
            if (err)
            {
                std::cout << "Error: " << voltek::reldb_get_error_text(err) << std::endl;
                return;
            }
        }

        auto gen_fname = [](const char* fname_exe) -> std::string
        {
            std::string p = "signs/";
            std::string fn = fname_exe;
            fn.erase(std::remove_if(fn.begin(), fn.end(), isspace), fn.end());
            p.append(fn).append(".relb");
            return p;
        };

        voltek::reldb_save_dev_file_patch(database, patch, gen_fname(szBuf).c_str());

        Sleep(10);
    }

    printf("Total signature [ total %u / fail %u ] (failed %u%%)\n", total_process, total_failed, ((total_failed * 100) / total_process));
}

extern "C" int zydis_decoder_init();
extern "C" void* zydis_create_instruction();
extern "C" int zydis_release_instruction(void* instruction);
extern "C" int zydis_init_instruction(void* instruction, void* raw);
extern "C" int zydis_instruction_length(void* instruction);
extern "C" int zydis_instruction_has_address(void* instruction);
extern "C" int zydis_instruction_opcode(void* instruction);
extern "C" unsigned __int64 zydis_instruction_attributes(void* instruction);

int main(int argc, char* argv[])
{
    printf("Program for recognizing and searching signatures from the old version to the new one.\nAuthor: perchik71\n");

    if (argc != 4)
    {
        printf(
            "Invalid arguments for call\n"
            "Example: VoltekLib.DBRebuilder.exe "
            "\"CreationKit_se_1_6_1130.exe\" "
            "\"CreationKitPlatformExtended_SSE_1_6_1130.database\" "
            "\"CreationKit_se_1_6_1378_1.exe\"\n");
        pause();
        return 0;
    }

    action_base_transform(argv[1], argv[2], argv[3]);

    //action_rebase("CreationKit.exe", "CreationKitPlatformExtended_FO4_1_10_162.database", true);
    
    /*action_base_transform(
        "CreationKit_f4_1_10_162.exe", 
        "CreationKitPlatformExtended_FO4_1_10_943_1.database", 
        "CreationKit_f4_1_10_943_1.exe");*/

   /* action_base_transform(
        "CreationKit_se_1_6_1130.exe",
        "CreationKitPlatformExtended_SSE_1_6_1130.database",
        "CreationKit_se_1_6_1378_1.exe");*/

    //if (!zydis_decoder_init())
    //{
    //    auto _instruction = zydis_create_instruction();
    //    if (_instruction)
    //    {
    //        //char ins_raw[] = "\xE8\xDA\xBC\xF3\xFF";
    //        //char ins_raw[] = "\x75\xF0";
    //        char ins_raw[] = "\xFF\x15\x18\x15\x80\x6B\x16\x48\x8D\x8E";
    //        //char ins_raw[] = "\x66\x44\x89\x44\x24\x38";
    //        //char ins_raw[] = "\xF7\x06\x00\x01\x00\x02";
    //        //char ins_raw[] = "\xF7\x86\xBC\x00\x00\x00\x00\x01\x00\x02";
    //        if (!zydis_init_instruction(_instruction, ins_raw))
    //        {
    //            auto ins = reinterpret_cast<ZydisDecodedInstruction*>(_instruction);
    //           
    //            printf("length: %u\n", ins->length);
    //            printf("opcode: 0x%X\n", ins->opcode);

    //            size_t len = zydis_instruction_length(_instruction);
    //            printf("has_addr: %u\n", zydis_instruction_has_address(_instruction));
    //            printf("mnemonic: %u\n", ins->mnemonic);
    //            printf("mnemonic_size: %u\n", (ZYAN_BITS_TO_REPRESENT(ins->mnemonic) + 7) / 8);
    //            printf("raw.disp.offset: %u\n", ins->raw.disp.offset);
    //            printf("raw.disp.size: %u\n", ins->raw.disp.size / 8);
    //            printf("raw.disp.value: 0x%llX\n", ins->raw.disp.value);

    //            for (int i = 0; i < 2; i++)
    //            {
    //                printf("raw.imm[%d].offset: %u\n", i, ins->raw.imm[i].offset);
    //                printf("raw.imm[%d].size: %u\n", i, ins->raw.imm[i].size / 8);
    //                printf("raw.imm[%d].value: 0x%llX\n", i, ins->raw.imm[i].value.s);
    //            }

    //            printf("raw.rex.offset: %u\n", ins->raw.rex.offset);

    //            printf("operand count: %u\n", ins->operand_count);
    //            for (ZyanU8 i = 0; i < ins->operand_count; i++)
    //            {
    //                auto operand = ins->operands[i];
    //                printf("operand type: %u\n", operand.type);
    //                printf("operand size: %u\n", operand.size / 8);
    //                if(operand.type == ZYDIS_OPERAND_TYPE_MEMORY)
    //                    printf("operand mem value: 0x%llX\n", operand.mem.disp.value);
    //                else if (operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
    //                    printf("operand imm value: 0x%llX\n", operand.imm.value.s);
    //            }
    //        }
    //        zydis_release_instruction(_instruction);
    //    }
    //}

    pause();
    return 0;
}