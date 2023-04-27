#include <planet/asset_manager.hpp>

#include <felspar/memory/hexdump.hpp>

#include <vorbis/codec.h>

#include <iostream>
#include <map>


namespace {
    struct decoder {
        vorbis_info vi;
        vorbis_comment vc;
        ogg_sync_state sync;
        ogg_stream_state stream;

        felspar::coro::generator<ogg_packet>
                vorbis_packets(std::vector<std::byte> ogg) {
            std::cout << "Decoding file size " << ogg.size() << " bytes\n";

            ogg_sync_init(&sync); // Always works
            vorbis_info_init(&vi);
            vorbis_comment_init(&vc);

            ogg_page op;
            if (ogg_sync_pageout(&sync, &op) != 0) {
                throw std::runtime_error{
                        "ogg_sync_pageout not requesting data"};
            }

            std::byte *const buffer = reinterpret_cast<std::byte *>(
                    ogg_sync_buffer(&sync, ogg.size()));
            std::copy(ogg.begin(), ogg.end(), buffer);
            if (ogg_sync_wrote(&sync, ogg.size()) == -1) {
                throw std::runtime_error{"ogg_sync_wrote failed"};
            }

            std::optional<int> streamid;
            while (ogg_sync_pageout(&sync, &op) == 1) {
                int serialno = ogg_page_serialno(&op);
                if (not streamid) {
                    streamid = serialno;
                    ogg_stream_init(&stream, serialno);
                }
                if (serialno == streamid) {
                    if (ogg_stream_pagein(&stream, &op) == -1) {
                        throw std::runtime_error{"ogg_stream_pagein failed"};
                    }

                    ogg_packet packet;
                    ogg_stream_packetout(&stream, &packet);
                    co_yield packet;
                }
            }
            std::cout << "No more packets\n";
        }
    };
}


int main(int const argc, char const *const argv[]) {
    std::cout << ".ogg file player\n";
    if (argc == 2) {
        std::cout << "Loading " << argv[1] << '\n';

        ::decoder decoder;
        auto packets =
                decoder.vorbis_packets(planet::file_loader::file_data(argv[1]));

        for (std::size_t expected{3}; expected; --expected) {
            auto vip = packets.next();
            if (vorbis_synthesis_headerin(&decoder.vi, &decoder.vc, &*vip)
                < 0) {
                throw std::runtime_error{"Not Vorbis audio data"};
            }
        }

        std::cout << "Bitstream is " << decoder.vi.channels << " channels at "
                  << decoder.vi.rate << "Hz\n";

        vorbis_dsp_state vd;
        vorbis_synthesis_init(&vd, &decoder.vi);
        vorbis_block vb;
        vorbis_block_init(&vd, &vb);

        std::size_t sample_length{};
        std::size_t packet_count{};

        for (auto &&packet : packets) {
            if (vorbis_synthesis(&vb, &packet) == 0) {
                vorbis_synthesis_blockin(&vd, &vb);
            } else {
                throw std::runtime_error{"vorbis_synthesis failed"};
            }
            float **pcm = nullptr;
            while (int samples = vorbis_synthesis_pcmout(&vd, &pcm)) {
                std::cout << "pcm: "
                          << felspar::memory::hexdump(std::span{
                                     reinterpret_cast<std::byte *>(pcm[0]),
                                     static_cast<std::size_t>(
                                             samples * sizeof(float))});
                vorbis_synthesis_read(&vd, samples);
                sample_length += samples;
            }
            ++packet_count;
        }

        std::cout << "Samples produced: " << sample_length << " ("
                  << (sample_length / decoder.vi.rate) << "s) over "
                  << packet_count << " ogg packets\n";

        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        ogg_stream_clear(&decoder.stream);
        vorbis_comment_clear(&decoder.vc);
        vorbis_info_clear(&decoder.vi);
        ogg_sync_clear(&decoder.sync);

        return 0;
    } else {
        std::cerr << "Usage\n\t" << argv[0] << " music.ogg\n";
        return 1;
    }
}
