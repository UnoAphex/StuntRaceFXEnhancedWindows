// Continuous, lossless runtime-data recorder for later renderer reconstruction.
// Generated sessions are ROM-derived local research data and are never packaged.
#pragma pack(push,1)
struct SrfRecordingHeader {
    char magic[8];
    uint32_t version;
    uint32_t header_bytes;
    uint64_t start_unix_ms;
    uint64_t rom_bytes;
    uint64_t rom_hash;
};
struct SrfRecordingFrame {
    char magic[4];
    uint32_t header_bytes;
    uint64_t frame;
    uint64_t performance_ticks;
    uint32_t input_mask;
    uint16_t source_width;
    uint16_t source_height;
    uint32_t flags;
    uint32_t chunk_count;
    uint64_t payload_bytes;
};
struct SrfRecordingChunk {
    uint32_t tag;
    uint32_t flags;
    uint32_t raw_bytes;
    uint32_t stored_bytes;
    uint64_t hash;
};
struct SrfRecordingEnd {
    char magic[4];
    uint32_t bytes;
    uint64_t final_frame;
    uint64_t frames_recorded;
};
#pragma pack(pop)

struct SrfRecordingChannel {
    uint32_t tag;
    std::vector<uint8_t> previous;
};
struct SrfPendingChunk {
    uint32_t tag;
    const uint8_t* data;
    uint32_t bytes;
    uint64_t hash;
    uint32_t state_tag;
    const uint8_t* state_data;
    uint32_t state_bytes;
};
struct SrfEncodedChunk {
    const SrfPendingChunk* source;
    std::vector<uint8_t> compressed;
    uint32_t flags;
};

static FILE* srf_recording_data;
static FILE* srf_recording_index;
static uint64_t srf_recording_frames;
static uint64_t srf_recording_start_ms;
static unsigned srf_recording_checkpoint_interval=300;
static std::vector<SrfRecordingChannel> srf_recording_channels;
static std::vector<uint8_t> srf_recording_checkpoint;
static std::vector<uint8_t> srf_recording_delta;
static COMPRESSOR_HANDLE srf_recording_compressor;

static uint32_t srf_recording_tag(char a,char b,char c,char d) {
    return static_cast<uint32_t>(static_cast<uint8_t>(a)) |
        (static_cast<uint32_t>(static_cast<uint8_t>(b))<<8) |
        (static_cast<uint32_t>(static_cast<uint8_t>(c))<<16) |
        (static_cast<uint32_t>(static_cast<uint8_t>(d))<<24);
}

static uint64_t srf_recording_hash(const uint8_t* data,size_t bytes) {
    uint64_t hash=1469598103934665603ull;
    for(size_t i=0;i<bytes;++i) hash=(hash^data[i])*1099511628211ull;
    return hash;
}

static bool srf_make_directory_tree(const std::string& path) {
    char full[MAX_PATH]{};
    if(!GetFullPathNameA(path.c_str(),MAX_PATH,full,nullptr)) return false;
    for(char* p=full+3;*p;++p) if(*p=='\\' || *p=='/') {
        const char saved=*p;*p='\0';CreateDirectoryA(full,nullptr);*p=saved;
    }
    return CreateDirectoryA(full,nullptr) || GetLastError()==ERROR_ALREADY_EXISTS;
}

static std::string srf_recording_root() {
    if(const char* configured=getenv("SRF_RECORDING_ROOT")) if(*configured) {
        char full[MAX_PATH]{};
        if(GetFullPathNameA(configured,MAX_PATH,full,nullptr)) return full;
    }
    char candidate[MAX_PATH]{};
    GetFullPathNameA((app_directory+"..\\..\\..\\recordings").c_str(),MAX_PATH,candidate,nullptr);
    std::string repository(candidate);
    // candidate itself is <repo>/recordings; its parent must be a Git checkout.
    std::string git_marker=repository+"\\..\\.git";
    if(GetFileAttributesA(git_marker.c_str())!=INVALID_FILE_ATTRIBUTES) return repository;
    return app_directory+"recordings";
}

static SrfRecordingChannel& srf_recording_channel(uint32_t tag) {
    for(auto& channel:srf_recording_channels) if(channel.tag==tag) return channel;
    srf_recording_channels.push_back({tag,{}});
    return srf_recording_channels.back();
}

static void srf_recording_consider(std::vector<SrfPendingChunk>& pending,uint32_t tag,
                                   const uint8_t* data,size_t bytes) {
    if(bytes>UINT32_MAX || (!data && bytes)) return;
    SrfRecordingChannel& channel=srf_recording_channel(tag);
    const uint64_t hash=data?srf_recording_hash(data,bytes):0;
    const bool changed=channel.previous.size()!=bytes ||
        (bytes && memcmp(channel.previous.data(),data,bytes)!=0);
    if(!changed) return;
    pending.push_back({tag,data,static_cast<uint32_t>(bytes),hash,
        tag,data,static_cast<uint32_t>(bytes)});
}

static void srf_recording_consider_delta(std::vector<SrfPendingChunk>& pending,
                                         uint32_t full_tag,uint32_t delta_tag,
                                         const uint8_t* data,size_t bytes) {
    if(bytes>UINT32_MAX || (!data && bytes)) return;
    SrfRecordingChannel& channel=srf_recording_channel(full_tag);
    if(channel.previous.size()!=bytes) {
        srf_recording_consider(pending,full_tag,data,bytes);
        return;
    }
    srf_recording_delta.clear();
    const uint32_t total=static_cast<uint32_t>(bytes),zero=0;
    srf_recording_delta.resize(8);
    memcpy(srf_recording_delta.data(),&total,4);
    memcpy(srf_recording_delta.data()+4,&zero,4);
    uint32_t spans=0;
    for(uint32_t offset=0;offset<total;offset+=256) {
        const uint32_t length=std::min<uint32_t>(256,total-offset);
        if(!memcmp(channel.previous.data()+offset,data+offset,length)) continue;
        const size_t old=srf_recording_delta.size();
        srf_recording_delta.resize(old+8+length);
        memcpy(srf_recording_delta.data()+old,&offset,4);
        memcpy(srf_recording_delta.data()+old+4,&length,4);
        memcpy(srf_recording_delta.data()+old+8,data+offset,length);
        ++spans;
    }
    if(!spans) return;
    memcpy(srf_recording_delta.data()+4,&spans,4);
    if(srf_recording_delta.size()>=bytes) {
        srf_recording_consider(pending,full_tag,data,bytes);
        return;
    }
    pending.push_back({delta_tag,srf_recording_delta.data(),
        static_cast<uint32_t>(srf_recording_delta.size()),
        srf_recording_hash(srf_recording_delta.data(),srf_recording_delta.size()),
        full_tag,data,static_cast<uint32_t>(bytes)});
}

static void srf_recording_core_blob(std::vector<SrfPendingChunk>& pending,uint32_t tag,
                                    srf_get_blob_t getter) {
    if(!getter) return;
    size_t bytes=0;const uint8_t* data=getter(&bytes);
    srf_recording_consider(pending,tag,data,bytes);
}

static bool srf_start_recording() {
    if(reconstruction_recording) return true;
    SYSTEMTIME now{};GetLocalTime(&now);
    char name[96]{};
    snprintf(name,sizeof(name),"session-%04u%02u%02u-%02u%02u%02u-p%lu",
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,
        static_cast<unsigned long>(GetCurrentProcessId()));
    const std::string root=srf_recording_root();
    reconstruction_session_directory=root+"\\"+name;
    if(!srf_make_directory_tree(reconstruction_session_directory)) return false;
    const std::string data_path=reconstruction_session_directory+"\\session.srfxrec";
    const std::string index_path=reconstruction_session_directory+"\\frames.csv";
    const std::string metadata_path=reconstruction_session_directory+"\\manifest.json";
    srf_recording_data=fopen(data_path.c_str(),"wb");
    srf_recording_index=fopen(index_path.c_str(),"wb");
    if(!srf_recording_data || !srf_recording_index) {
        if(srf_recording_data) fclose(srf_recording_data);
        if(srf_recording_index) fclose(srf_recording_index);
        srf_recording_data=srf_recording_index=nullptr;
        return false;
    }
    FILE* metadata=fopen(metadata_path.c_str(),"wb");
    FILETIME file_time{};GetSystemTimeAsFileTime(&file_time);
    ULARGE_INTEGER stamp{};stamp.LowPart=file_time.dwLowDateTime;stamp.HighPart=file_time.dwHighDateTime;
    srf_recording_start_ms=(stamp.QuadPart-116444736000000000ull)/10000ull;
    const uint64_t rom_hash=game_rom.empty()?0:srf_recording_hash(game_rom.data(),game_rom.size());
    SrfRecordingHeader header{{'S','R','F','X','R','E','C','\0'},2,sizeof(SrfRecordingHeader),
        srf_recording_start_ms,game_rom.size(),rom_hash};
    fwrite(&header,1,sizeof(header),srf_recording_data);
    fprintf(srf_recording_index,"frame,qpc_ticks,input_mask,race,scene_changed,width,height,file_offset,frame_bytes,changed_chunks,polygon_bytes,display_face_bytes,checkpoint\r\n");
    if(metadata) {
        fprintf(metadata,
            "{\n  \"format\": \"Stunt Race FX reconstruction session\",\n  \"version\": 2,\n"
            "  \"build\": \"v4.15\",\n  \"started_unix_ms\": %llu,\n"
            "  \"rom_bytes\": %llu,\n  \"rom_fnv1a64\": \"%016llX\",\n"
            "  \"deduplication\": \"unchanged chunks inherit the previous value\",\n"
            "  \"compression\": \"Windows XPRESS Huffman when smaller\",\n"
            "  \"container\": \"session.srfxrec\",\n  \"index\": \"frames.csv\",\n"
            "  \"notice\": \"Local ROM-derived research data; do not distribute or commit\"\n}\n",
            static_cast<unsigned long long>(srf_recording_start_ms),
            static_cast<unsigned long long>(game_rom.size()),
            static_cast<unsigned long long>(rom_hash));
        fclose(metadata);
    }
    srf_recording_channels.clear();srf_recording_frames=0;
    CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&srf_recording_compressor);
    if(const char* interval=getenv("SRF_RECORD_CHECKPOINT_INTERVAL"))
        srf_recording_checkpoint_interval=std::max(1,atoi(interval));
    reconstruction_recording=true;
    fprintf(stderr,"Reconstruction recorder: %s\n",reconstruction_session_directory.c_str());
    return true;
}

static void srf_stop_recording() {
    if(!reconstruction_recording && !srf_recording_data && !srf_recording_index) return;
    if(srf_recording_data) {
        const SrfRecordingEnd end{{'E','N','D','!'},sizeof(SrfRecordingEnd),emulated_frame,srf_recording_frames};
        fwrite(&end,1,sizeof(end),srf_recording_data);fflush(srf_recording_data);fclose(srf_recording_data);
    }
    if(srf_recording_index) {fflush(srf_recording_index);fclose(srf_recording_index);}
    srf_recording_data=srf_recording_index=nullptr;
    if(srf_recording_compressor) CloseCompressor(srf_recording_compressor);
    srf_recording_compressor=nullptr;
    reconstruction_recording=false;
    srf_recording_channels.clear();
}

static void srf_record_frame() {
    if(!reconstruction_recording || !srf_recording_data || !srf_recording_index) return;
    std::vector<SrfPendingChunk> pending;pending.reserve(20);
    srf_recording_core_blob(pending,srf_recording_tag('P','O','L','Y'),core_srf_get_wide_capture);
    srf_recording_core_blob(pending,srf_recording_tag('C','A','M','R'),core_srf_get_wide_camera);
    srf_recording_consider(pending,srf_recording_tag('D','I','S','P'),compat_wide_display_bytes.data(),compat_wide_display_bytes.size());
    srf_recording_core_blob(pending,srf_recording_tag('C','G','R','M'),core_srf_get_cgram);
    srf_recording_consider(pending,srf_recording_tag('C','O','L','R'),reinterpret_cast<const uint8_t*>(compat_wide_colors.data()),sizeof(compat_wide_colors));
    srf_recording_core_blob(pending,srf_recording_tag('P','P','U','R'),core_srf_get_wide_ppu);
    srf_recording_core_blob(pending,srf_recording_tag('V','R','A','M'),core_srf_get_vram);
    srf_recording_core_blob(pending,srf_recording_tag('D','M','A','S'),core_srf_get_wide_dma);
    srf_recording_core_blob(pending,srf_recording_tag('B','K','G','D'),core_srf_get_wide_background);
    srf_recording_core_blob(pending,srf_recording_tag('W','R','E','F'),core_srf_get_wide_world_reference);
    srf_recording_core_blob(pending,srf_recording_tag('S','P','R','T'),core_srf_get_wide_sprites);
    srf_recording_core_blob(pending,srf_recording_tag('G','R','A','M'),core_srf_get_gsu_ram);
    srf_recording_core_blob(pending,srf_recording_tag('G','R','E','G'),core_srf_get_gsu_registers);
    srf_recording_core_blob(pending,srf_recording_tag('G','T','R','C'),core_srf_get_gsu_trace);
    srf_recording_core_blob(pending,srf_recording_tag('R','W','R','T'),core_srf_get_ram_write_trace);
    const uint8_t* wram=static_cast<const uint8_t*>(core_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    const size_t wram_bytes=core_retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    srf_recording_consider_delta(pending,srf_recording_tag('W','R','A','M'),
        srf_recording_tag('W','D','E','L'),wram,wram_bytes);
    if(!source_pixels.empty()) srf_recording_consider(pending,srf_recording_tag('F','B','U','F'),
        reinterpret_cast<const uint8_t*>(source_pixels.data()),source_pixels.size()*sizeof(uint32_t));
    if(!compat_hd_overlay_pixels.empty()) srf_recording_consider(pending,srf_recording_tag('H','O','V','R'),
        reinterpret_cast<const uint8_t*>(compat_hd_overlay_pixels.data()),compat_hd_overlay_pixels.size()*sizeof(uint32_t));
    const bool checkpoint=srf_recording_frames==0 ||
        (srf_recording_checkpoint_interval && emulated_frame%srf_recording_checkpoint_interval==0);
    if(checkpoint) {
        const size_t bytes=core_retro_serialize_size();srf_recording_checkpoint.resize(bytes);
        if(bytes && core_retro_serialize(srf_recording_checkpoint.data(),bytes))
            srf_recording_consider(pending,srf_recording_tag('S','T','A','T'),srf_recording_checkpoint.data(),bytes);
    }
    std::vector<SrfEncodedChunk> encoded;encoded.reserve(pending.size());
    uint64_t payload=0,polygon_bytes=0,display_bytes=0;
    for(const auto& chunk:pending) {
        encoded.push_back({&chunk,{},0});
        SrfEncodedChunk& output=encoded.back();
        if(srf_recording_compressor && chunk.bytes>=256) {
            SIZE_T required=0;
            Compress(srf_recording_compressor,chunk.data,chunk.bytes,nullptr,0,&required);
            if(required) {
                output.compressed.resize(required);SIZE_T written=0;
                if(Compress(srf_recording_compressor,chunk.data,chunk.bytes,
                    output.compressed.data(),output.compressed.size(),&written) && written<chunk.bytes) {
                    output.compressed.resize(written);output.flags=1;
                } else output.compressed.clear();
            }
        }
        const size_t stored=output.flags?output.compressed.size():chunk.bytes;
        payload+=sizeof(SrfRecordingChunk)+stored;
        if(chunk.tag==srf_recording_tag('P','O','L','Y')) polygon_bytes=chunk.bytes;
        if(chunk.tag==srf_recording_tag('D','I','S','P')) display_bytes=chunk.bytes;
    }
    const int64_t offset=_ftelli64(srf_recording_data);
    uint32_t flags=(race_scene_active?1u:0u)|(visual_frame_changed?2u:0u)|
        (compat_wide?4u:0u)|(compat_hd_center?8u:0u)|(checkpoint?16u:0u);
    const SrfRecordingFrame frame{{'F','R','A','M'},sizeof(SrfRecordingFrame),emulated_frame,
        SDL_GetPerformanceCounter(),reported_input_mask,static_cast<uint16_t>(source_width),
        static_cast<uint16_t>(source_height),flags,static_cast<uint32_t>(pending.size()),payload};
    fwrite(&frame,1,sizeof(frame),srf_recording_data);
    for(const auto& output:encoded) {
        const SrfPendingChunk& chunk=*output.source;
        const uint32_t stored=output.flags?static_cast<uint32_t>(output.compressed.size()):chunk.bytes;
        const SrfRecordingChunk header{chunk.tag,output.flags,chunk.bytes,stored,chunk.hash};
        fwrite(&header,1,sizeof(header),srf_recording_data);
        if(stored) fwrite(output.flags?output.compressed.data():chunk.data,1,stored,srf_recording_data);
        SrfRecordingChannel& channel=srf_recording_channel(chunk.state_tag);
        if(chunk.state_bytes) channel.previous.assign(chunk.state_data,chunk.state_data+chunk.state_bytes);
        else channel.previous.clear();
    }
    const uint64_t frame_bytes=sizeof(frame)+payload;
    fprintf(srf_recording_index,"%llu,%llu,%04X,%d,%d,%u,%u,%lld,%llu,%u,%llu,%llu,%d\r\n",
        static_cast<unsigned long long>(emulated_frame),
        static_cast<unsigned long long>(frame.performance_ticks),reported_input_mask,
        race_scene_active?1:0,visual_frame_changed?1:0,source_width,source_height,
        static_cast<long long>(offset),static_cast<unsigned long long>(frame_bytes),
        static_cast<unsigned>(pending.size()),static_cast<unsigned long long>(polygon_bytes),
        static_cast<unsigned long long>(display_bytes),checkpoint?1:0);
    ++srf_recording_frames;
    if((srf_recording_frames%60)==0) {fflush(srf_recording_data);fflush(srf_recording_index);}
}
