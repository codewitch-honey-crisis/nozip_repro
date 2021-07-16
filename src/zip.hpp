#include "bits.hpp"
#include "stream.hpp"
#include "stb_inflate.h"
namespace zip {
    static_assert(bits::endianness()!=bits::endian_mode::none,"Please define HTCW_LITTLE_ENDIAN or HTCW_BIG_ENDIAN before including zip to indicate the byte order of the platform.");
    enum struct zip_result {
        success = 0,
        invalid_argument = 1,
        invalid_archive = 2,
        not_supported = 3,
        io_error = 4,
        invalid_state = 5
    };
    namespace helpers {
        #pragma pack(1)
        struct local_file_header final { 
            uint32_t signature; 
            uint16_t version_needed; 
            uint16_t flags; 
            uint16_t compression_method; 
            uint16_t last_mod_file_time; 
            uint16_t last_mod_file_date; 
            uint32_t crc_32; 
            uint32_t compressed_size; 
            uint32_t uncompressed_size; 
            uint16_t file_name_length; 
            uint16_t extra_field_length; 
        };
        #pragma pop(pack)
        #pragma pack(1)
        struct central_dir_header {
            uint32_t signature;
            uint16_t version;
            uint16_t version_needed;
            uint16_t flags;
            uint16_t compression_method;
            uint16_t last_mod_file_time;
            uint16_t last_mod_file_date;
            uint32_t crc_32;
            uint32_t compressed_size;
            uint32_t uncompressed_size;
            uint16_t file_name_length;
            uint16_t extra_field_length;
            uint16_t file_comment_length;
            uint16_t disk_number_start;
            uint16_t internal_file_attributes;
            uint32_t external_file_attributes;
            uint32_t local_header_offset;
        };
        #pragma pop(pack)
        #pragma pack(1)
        struct end_of_central_dir_record64 {
            uint32_t signature;
            uint64_t eocdr_size;
            uint16_t version;
            uint16_t version_needed;
            uint32_t disk_number;
            uint32_t cdr_disk_number;
            uint64_t disk_num_entries;
            uint64_t num_entries;
            uint64_t cdr_size;
            uint64_t cdr_offset;
        };
        #pragma pop(pack)
        #pragma pack(1)
        struct end_of_central_dir_locator64 {
            uint32_t signature;
            uint32_t eocdr_disk;
            uint64_t eocdr_offset;
            uint32_t num_disks;
        };
        #pragma pop(pack)
        #pragma pack(1)
        struct end_of_central_dir_record {
            uint32_t signature;
            uint16_t disk_number;
            uint16_t cdr_disk_number;
            uint16_t disk_num_entries;
            uint16_t num_entries;
            uint32_t cdr_size;
            uint32_t cdr_offset;
            uint16_t ZIP_file_comment_length;
        };
        #pragma pop(pack)
    }
    class archive;
    class archive_entry;
    class archive_stream final : public io::stream {
        friend class archive_entry;
        stb::stbi__stream m_stb_stream;
        io::stream* m_stream;
        long long int m_begin;
        long long int m_offset;
        long long int m_size;
        archive_stream() : m_stream(nullptr),m_offset(0) {
            
        }
    public:
        virtual int getc() {
            uint8_t b;
            if(1==read(&b,1)) {
                return b;
            }
            return -1;
        }
        virtual int putc(int value) {
            return -1;
        }
        virtual size_t read(uint8_t* destination,size_t size) {
            if(nullptr==m_stream || nullptr==destination || size==0 || m_offset>=m_size)
                return 0;
            if(size+m_offset>=m_size) {
                size = m_size-m_offset;
            }
            m_stb_stream.start_in = destination;
            m_stb_stream
            return size;
        }
        
        virtual size_t write(const uint8_t* source,size_t size) {
            return 0;
        }
        
        virtual unsigned long long seek(long long position,io::seek_origin origin=io::seek_origin::start)  {
            return 0;
        }
        virtual io::stream_caps caps() const {
            io::stream_caps caps;
            caps.read = 1;
            caps.seek = 0;
            caps.write = 0;
            return caps;
        }
    };
    class archive_entry final {
        friend class archive;
        long long int m_offset;
        long long int m_local_header_offset;
        size_t m_compressed_size;
        size_t m_uncompressed_size;
        io::stream* m_stream;
    public:
        archive_entry() : m_offset(0), m_stream(nullptr) {

        }
        inline bool initialized() const {
            return nullptr!=m_stream;
        }
        size_t copy_path(char* buffer,size_t size) const {
            if(nullptr==buffer || nullptr==m_stream || size==0) return 0;
            m_stream->seek(m_offset);
            helpers::central_dir_header cdh;
            if(sizeof(cdh)!=m_stream->read((uint8_t*)&cdh,sizeof(cdh))) {
                return 0;
            }
            size_t s = size<(cdh.file_name_length+1)?size:(cdh.file_name_length+1);
            s=m_stream->read((uint8_t*)buffer,s-1);
            buffer[s]='\0';
            return s;
        }
        inline size_t uncompressed_size() const {
            return m_uncompressed_size;
        }
        inline size_t compressed_size() const {
            return m_compressed_size;
        }
        archive_stream stream() const {
            archive_stream result;
            m_stream->seek(m_local_header_offset,io::seek_origin::start);
            helpers::local_file_header lfh;
            if(sizeof(lfh)==m_stream->read((uint8_t*)&lfh,sizeof(lfh))) {
                result.m_begin = m_local_header_offset+sizeof(lfh)+lfh.file_name_length+lfh.extra_field_length;
                result.m_size = m_compressed_size;
                result.m_stream = m_stream;
            }
            return result;
        }
    };
    class archive final {
        size_t m_entries_size;
        long long int m_offset;
        bool m_is64;
        io::stream* m_stream;
    
        zip_result init(io::stream *stream) {
            if(nullptr!=m_stream) {
                return zip_result::invalid_state;
            }
            if(nullptr==stream || stream->caps().read==0 || stream->caps().seek==0) {
                return zip_result::invalid_argument;
            }
            // find the end of central directory record
            uint32_t signature;
            size_t offset;

            for (offset = sizeof(helpers::end_of_central_dir_record);; ++offset) {
                if(offset>UINT16_MAX) {
                    return zip_result::invalid_archive;
                }
                stream->seek(-offset,io::seek_origin::end);
                if(sizeof(signature)!=stream->read((uint8_t*)&signature,sizeof(signature))) {
                    return zip_result::io_error;
                }
                if (signature == 0x06054B50)
                    break;
            }

            // read end of central directory record
            helpers::end_of_central_dir_record eocdr;
            stream->seek(-offset,io::seek_origin::end);
            if(sizeof(eocdr)!=stream->read((uint8_t*)&eocdr,sizeof(eocdr))) {
                return zip_result::io_error;
            }
            if (!(eocdr.signature == 0x06054B50 &&
                eocdr.disk_number == 0 &&
                eocdr.cdr_disk_number == 0 &&
                eocdr.disk_num_entries == eocdr.num_entries)) {
                return zip_result::not_supported;
            }

            // check for zip64
            helpers::end_of_central_dir_record64 eocdr64;
            m_is64 = eocdr.num_entries == UINT16_MAX || eocdr.cdr_offset == UINT32_MAX || eocdr.cdr_size == UINT32_MAX;
            if (m_is64) {
                // zip64 end of central directory locator
                helpers::end_of_central_dir_locator64 eocdl64;
                stream->seek(-offset-sizeof(eocdl64),io::seek_origin::end);
                if(sizeof(eocdl64)!=stream->read((uint8_t*)&eocdl64,sizeof(eocdl64))) {
                    return zip_result::io_error;
                }
                if (!(eocdl64.signature == bits::from_le(0x07064B50) &&
                    eocdl64.eocdr_disk == 0 &&
                    eocdl64.num_disks == 1)) {
                    return zip_result::not_supported;
                }
                // zip64 end of central directory record
                stream->seek(eocdl64.eocdr_offset,io::seek_origin::start);
                if(sizeof(eocdr64)!=stream->read((uint8_t*)&eocdr64,sizeof(eocdr64))) {
                    return zip_result::io_error;
                }
                if (!(eocdr64.signature == bits::from_le(0x06064B50) &&
                    eocdr64.disk_number == 0 &&
                    eocdr64.cdr_disk_number == 0 &&
                    eocdr64.disk_num_entries == eocdr64.num_entries)) {
                    return zip_result::not_supported;
                }
            }

            // store the offset to the central directory record
            // and the number of entries
            if(m_is64) {
                m_offset = eocdr64.cdr_offset;
                m_entries_size = eocdr64.num_entries;
            } else {
                m_offset = eocdr.cdr_offset;
                m_entries_size = eocdr.num_entries;
            }
            m_offset = m_is64?eocdr64.cdr_offset:eocdr.cdr_offset;
            m_entries_size = m_is64?eocdr64.num_entries:eocdr.num_entries;
            m_stream = stream;
            return zip_result::success;   
        }
public:
        archive() : m_stream(nullptr) {

        }
        archive(io::stream* stream) {
            init(stream);
        }
        static zip_result open(io::stream* stream,archive* out_archive) {
            if(nullptr==out_archive || nullptr==stream) {
                return zip_result::invalid_argument;
            }
            return out_archive->init(stream);
        }
        inline bool initialized() const {
            return nullptr!=m_stream;
        }
        inline size_t entries_size() const {
            return m_entries_size;
        }
        zip_result entry(size_t index,archive_entry* out_entry) {
            if(nullptr==out_entry) {
                return zip_result::invalid_argument;
            }
            if(nullptr==m_stream) {
                return zip_result::invalid_state;
            }
            if(index>=m_entries_size) {
                return zip_result::invalid_argument;
            }

            m_stream->seek(m_offset,io::seek_origin::start);
            long long int offs = m_offset;
            for(size_t i = 0;i<index;++i) {
                helpers::central_dir_header cdh;
                if(sizeof(cdh)!=m_stream->read((uint8_t*)&cdh,sizeof(cdh))) {
                    return zip_result::io_error;
                }
                offs+=sizeof(cdh);
                out_entry->m_compressed_size=cdh.compressed_size;
                out_entry->m_uncompressed_size= cdh.uncompressed_size;
                out_entry->m_local_header_offset = cdh.local_header_offset;
                long long int adv = cdh.file_name_length+cdh.extra_field_length+cdh.file_comment_length;
                offs+=adv;
                m_stream->seek(adv,io::seek_origin::current);
            }
            out_entry->m_offset = offs;
            out_entry->m_stream = m_stream;
            return zip_result::success;
        }

    };
}