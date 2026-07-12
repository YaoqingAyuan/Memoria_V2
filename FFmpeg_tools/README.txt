FFmpeg 64-bit static Windows build from www.gyan.dev

Version: 2026-07-06-git-c6498178bb-full_build-www.gyan.dev

License: GPL v3

Source Code: https://github.com/FFmpeg/FFmpeg/commit/c6498178bb

External Assets
frei0r plugins:   https://www.gyan.dev/ffmpeg/builds/ffmpeg-frei0r-plugins
lensfun database: https://www.gyan.dev/ffmpeg/builds/ffmpeg-lensfun-db
whisper models:   https://huggingface.co/ggerganov/whisper.cpp/tree/main

git-full build configuration: 

ARCH                      x86 (generic)
big-endian                no
runtime cpu detection     yes
standalone assembly       yes
x86 assembler             nasm
MMX enabled               yes
MMXEXT enabled            yes
SSE enabled               yes
SSSE3 enabled             yes
AESNI enabled             yes
CLMUL enabled             yes
AVX enabled               yes
AVX2 enabled              yes
AVX-512 enabled           yes
AVX-512ICL enabled        yes
XOP enabled               yes
FMA3 enabled              yes
FMA4 enabled              yes
i686 features enabled     yes
CMOV is fast              yes
EBX available             yes
EBP available             yes
debug symbols             yes
strip symbols             yes
optimize for size         no
optimizations             yes
static                    yes
shared                    no
network support           yes
threading support         pthreads
safe bitstream reader     yes
texi2html enabled         no
perl enabled              yes
pod2man enabled           yes
makeinfo enabled          yes
makeinfo supports HTML    yes
experimental features     yes
xmllint enabled           yes

External libraries:
avisynth                libgsm                  libsvtav1
bzlib                   libharfbuzz             libsvtjpegxs
cairo                   libilbc                 libtheora
chromaprint             libjxl                  libtwolame
frei0r                  liblc3                  libuavs3d
gmp                     liblensfun              libvidstab
gnutls                  libmodplug              libvmaf
iconv                   libmp3lame              libvo_amrwbenc
ladspa                  libmysofa               libvorbis
lcms2                   liboapv                 libvpx
libaom                  libopencore_amrnb       libvvenc
libaribb24              libopencore_amrwb       libwebp
libaribcaption          libopenjpeg             libx264
libass                  libopenmpt              libx265
libbluray               libopus                 libxavs2
libbs2b                 libplacebo              libxevd
libcaca                 libqrencode             libxeve
libcdio                 libquirc                libxml2
libcodec2               librav1e                libxvid
libdav1d                librist                 libzimg
libdavs2                librubberband           libzmq
libdvdnav               libshaderc              libzvbi
libdvdread              libshine                lzma
libflite                libsnappy               mediafoundation
libfontconfig           libsoxr                 openal
libfreetype             libspeex                sdl2
libfribidi              libsrt                  whisper
libgme                  libssh                  zlib

External libraries providing hardware acceleration:
amf                     d3d12va                 nvdec
cuda                    dxva2                   nvenc
cuda_llvm               ffnvcodec               opencl
cuvid                   libmfx                  vaapi
d3d11va                 libvpl                  vulkan

Libraries:
avcodec                 avformat                swscale
avdevice                avutil
avfilter                swresample

Programs:
ffmpeg                  ffplay                  ffprobe

Enabled decoders:
aac                     flv                     pcm_u24be
aac_fixed               fmvc                    pcm_u24le
aac_latm                fourxm                  pcm_u32be
aasc                    fraps                   pcm_u32le
ac3                     frwu                    pcm_u8
ac3_fixed               ftr                     pcm_vidc
acelp_kelvin            g2m                     pcx
adpcm_4xm               g723_1                  pdv
adpcm_adx               g728                    pfm
adpcm_afc               g729                    pgm
adpcm_agm               gdv                     pgmyuv
adpcm_aica              gem                     pgssub
adpcm_argo              gif                     pgx
adpcm_circus            gremlin_dpcm            phm
adpcm_ct                gsm                     photocd
adpcm_dtk               gsm_ms                  pictor
adpcm_ea                h261                    pixlet
adpcm_ea_maxis_xa       h263                    pjs
adpcm_ea_r1             h263i                   png
adpcm_ea_r2             h263p                   ppm
adpcm_ea_r3             h264                    prores
adpcm_ea_xas            h264_amf                prores_raw
adpcm_g722              h264_cuvid              prosumer
adpcm_g726              h264_qsv                psd
adpcm_g726le            hap                     ptx
adpcm_ima_acorn         hca                     qcelp
adpcm_ima_alp           hcom                    qdm2
adpcm_ima_amv           hdr                     qdmc
adpcm_ima_apc           hevc                    qdraw
adpcm_ima_apm           hevc_amf                qoa
adpcm_ima_cunning       hevc_cuvid              qoi
adpcm_ima_dat4          hevc_qsv                qpeg
adpcm_ima_dk3           hnm4_video              qtrle
adpcm_ima_dk4           hq_hqa                  r10k
adpcm_ima_ea_eacs       hqx                     r210
adpcm_ima_ea_sead       huffyuv                 ra_144
adpcm_ima_escape        hymt                    ra_288
adpcm_ima_hvqm2         iac                     ralf
adpcm_ima_hvqm4         idcin                   rasc
adpcm_ima_iss           idf                     rawvideo
adpcm_ima_magix         iff_ilbm                realtext
adpcm_ima_moflex        ilbc                    rka
adpcm_ima_mtf           imc                     rl2
adpcm_ima_oki           imm4                    roq
adpcm_ima_pda           imm5                    roq_dpcm
adpcm_ima_qt            indeo2                  rpza
adpcm_ima_rad           indeo3                  rscc
adpcm_ima_smjpeg        indeo4                  rtv1
adpcm_ima_ssi           indeo5                  rv10
adpcm_ima_wav           interplay_acm           rv20
adpcm_ima_ws            interplay_dpcm          rv30
adpcm_ima_xbox          interplay_video         rv40
adpcm_ms                ipu                     rv60
adpcm_mtaf              jacosub                 s302m
adpcm_n64               jpeg2000                sami
adpcm_psx               jpegls                  sanm
adpcm_psxc              jv                      sbc
adpcm_sanyo             kgv1                    scpr
adpcm_sbpro_2           kmvc                    screenpresso
adpcm_sbpro_3           lagarith                sdx2_dpcm
adpcm_sbpro_4           lead                    sga
adpcm_swf               libaom_av1              sgi
adpcm_thp               libaribb24              sgirle
adpcm_thp_le            libaribcaption          sheervideo
adpcm_vima              libcodec2               shorten
adpcm_xa                libdav1d                simbiosis_imx
adpcm_xmd               libdavs2                sipr
adpcm_yamaha            libgsm                  siren
adpcm_zork              libgsm_ms               smackaud
agm                     libilbc                 smacker
ahx                     libjxl                  smc
aic                     libjxl_anim             smvjpeg
alac                    liblc3                  snow
alias_pix               libopencore_amrnb       sol_dpcm
als                     libopencore_amrwb       sp5x
amrnb                   libopus                 speedhq
amrwb                   libspeex                speex
amv                     libsvtjpegxs            srgc
anm                     libuavs3d               srt
ansi                    libvorbis               ssa
anull                   libvpx_vp8              stl
apac                    libvpx_vp9              subrip
ape                     libxevd                 subviewer
apng                    libzvbi_teletext        subviewer1
aptx                    loco                    sunrast
aptx_hd                 lscr                    svq1
apv                     m101                    svq3
arbc                    mace3                   tak
argo                    mace6                   targa
ass                     magicyuv                targa_y216
asv1                    mdec                    tdsc
asv2                    media100                text
atrac1                  metasound               theora
atrac3                  microdvd                thp
atrac3al                mimic                   tiertexseqvideo
atrac3p                 misc4                   tiff
atrac3pal               mjpeg                   tmv
atrac9                  mjpeg_cuvid             truehd
aura                    mjpeg_qsv               truemotion1
aura2                   mjpegb                  truemotion2
av1                     mlp                     truemotion2rt
av1_amf                 mmvideo                 truespeech
av1_cuvid               mobiclip                tscc
av1_qsv                 motionpixels            tscc2
avrn                    movtext                 tta
avrp                    mp1                     twinvq
avs                     mp1float                txd
avui                    mp2                     ulti
bethsoftvid             mp2float                utvideo
bfi                     mp3                     v210
bink                    mp3adu                  v210x
binkaudio_dct           mp3adufloat             vb
binkaudio_rdft          mp3float                vble
bintext                 mp3on4                  vbn
bitpacked               mp3on4float             vc1
bmp                     mpc7                    vc1_cuvid
bmv_audio               mpc8                    vc1_qsv
bmv_video               mpeg1_cuvid             vc1image
bonk                    mpeg1video              vcr1
brender_pix             mpeg2_cuvid             vmdaudio
c93                     mpeg2_qsv               vmdvideo
cavs                    mpeg2video              vmix
cbd2_dpcm               mpeg4                   vmnc
ccaption                mpeg4_cuvid             vnull
cdgraphics              mpegvideo               vorbis
cdtoons                 mpl2                    vp3
cdxl                    msa1                    vp4
cfhd                    mscc                    vp5
cinepak                 msmpeg4v1               vp6
clearvideo              msmpeg4v2               vp6a
cljr                    msmpeg4v3               vp6f
cllc                    msnsiren                vp7
comfortnoise            msp2                    vp8
cook                    msrle                   vp8_cuvid
cpia                    mss1                    vp8_qsv
cri                     mss2                    vp9
cscd                    msvideo1                vp9_amf
cyuv                    mszh                    vp9_cuvid
dca                     mts2                    vp9_qsv
dds                     mv30                    vplayer
derf_dpcm               mvc1                    vqa
dfa                     mvc2                    vqc
dfpwm                   mvdv                    vvc
dirac                   mvha                    vvc_qsv
dnxhd                   mwsc                    wady_dpcm
dolby_e                 mxpeg                   wavarc
dpx                     nellymoser              wavpack
dsd_lsbf                notchlc                 wbmp
dsd_lsbf_planar         nuv                     wcmv
dsd_msbf                on2avc                  webp
dsd_msbf_planar         opus                    webp_anim
dsicinaudio             osq                     webvtt
dsicinvideo             paf_audio               wmalossless
dss_sp                  paf_video               wmapro
dst                     pam                     wmav1
dvaudio                 pbm                     wmav2
dvbsub                  pcm_alaw                wmavoice
dvdsub                  pcm_bluray              wmv1
dvvideo                 pcm_dvd                 wmv2
dxa                     pcm_f16le               wmv3
dxtory                  pcm_f24le               wmv3image
dxv                     pcm_f32be               wnv1
eac3                    pcm_f32le               wrapped_avframe
eacmv                   pcm_f64be               ws_snd1
eamad                   pcm_f64le               xan_dpcm
eatgq                   pcm_lxf                 xan_wc3
eatgv                   pcm_mulaw               xan_wc4
eatqi                   pcm_s16be               xbin
eightbps                pcm_s16be_planar        xbm
eightsvx_exp            pcm_s16le               xface
eightsvx_fib            pcm_s16le_planar        xl
escape124               pcm_s24be               xma1
escape130               pcm_s24daud             xma2
evrc                    pcm_s24le               xpm
exr                     pcm_s24le_planar        xsub
fastaudio               pcm_s32be               xwd
ffv1                    pcm_s32le               y41p
ffvhuff                 pcm_s32le_planar        ylc
ffwavesynth             pcm_s64be               yop
fic                     pcm_s64le               yuv4
fits                    pcm_s8                  zero12v
flac                    pcm_s8_planar           zerocodec
flashsv                 pcm_sga                 zlib
flashsv2                pcm_u16be               zmbv
flic                    pcm_u16le

Enabled encoders:
a64multi                hevc_d3d12va            pcm_s32be
a64multi5               hevc_mf                 pcm_s32le
aac                     hevc_nvenc              pcm_s32le_planar
aac_mf                  hevc_qsv                pcm_s64be
ac3                     hevc_vaapi              pcm_s64le
ac3_fixed               hevc_vulkan             pcm_s8
ac3_mf                  huffyuv                 pcm_s8_planar
adpcm_adx               jpeg2000                pcm_u16be
adpcm_argo              jpegls                  pcm_u16le
adpcm_g722              libaom_av1              pcm_u24be
adpcm_g726              libcodec2               pcm_u24le
adpcm_g726le            libgsm                  pcm_u32be
adpcm_ima_alp           libgsm_ms               pcm_u32le
adpcm_ima_amv           libilbc                 pcm_u8
adpcm_ima_apm           libjxl                  pcm_vidc
adpcm_ima_qt            libjxl_anim             pcx
adpcm_ima_ssi           liblc3                  pdv
adpcm_ima_wav           libmp3lame              pfm
adpcm_ima_ws            liboapv                 pgm
adpcm_ms                libopencore_amrnb       pgmyuv
adpcm_swf               libopenjpeg             phm
adpcm_yamaha            libopus                 png
alac                    librav1e                ppm
alias_pix               libshine                prores
amv                     libspeex                prores_aw
anull                   libsvtav1               prores_ks
apng                    libsvtjpegxs            prores_ks_vulkan
aptx                    libtheora               qoi
aptx_hd                 libtwolame              qtrle
apv_vulkan              libvo_amrwbenc          r10k
ass                     libvorbis               r210
asv1                    libvpx_vp8              ra_144
asv2                    libvpx_vp9              rawvideo
av1_amf                 libvvenc                roq
av1_d3d12va             libwebp                 roq_dpcm
av1_mf                  libwebp_anim            rpza
av1_nvenc               libx264                 rv10
av1_qsv                 libx264rgb              rv20
av1_vaapi               libx265                 s302m
av1_vulkan              libxavs2                sbc
avrp                    libxeve                 sgi
avui                    libxvid                 smc
bitpacked               ljpeg                   snow
bmp                     magicyuv                speedhq
cfhd                    mjpeg                   srt
cinepak                 mjpeg_qsv               ssa
cljr                    mjpeg_vaapi             subrip
comfortnoise            mlp                     sunrast
dca                     movtext                 svq1
dfpwm                   mp2                     targa
dnxhd                   mp2fixed                text
dpx                     mp3_mf                  tiff
dvbsub                  mpeg1video              truehd
dvdsub                  mpeg2_qsv               tta
dvvideo                 mpeg2_vaapi             ttml
dxv                     mpeg2video              utvideo
eac3                    mpeg4                   v210
exr                     msmpeg4v2               vbn
ffv1                    msmpeg4v3               vc2
ffv1_vulkan             msrle                   vnull
ffvhuff                 msvideo1                vorbis
fits                    nellymoser              vp8_vaapi
flac                    opus                    vp9_qsv
flashsv                 pam                     vp9_vaapi
flashsv2                pbm                     wavpack
flv                     pcm_alaw                wbmp
g723_1                  pcm_bluray              webvtt
gif                     pcm_dvd                 wmav1
h261                    pcm_f32be               wmav2
h263                    pcm_f32le               wmv1
h263p                   pcm_f64be               wmv2
h264_amf                pcm_f64le               wrapped_avframe
h264_d3d12va            pcm_mulaw               xbm
h264_mf                 pcm_s16be               xface
h264_nvenc              pcm_s16be_planar        xsub
h264_qsv                pcm_s16le               xwd
h264_vaapi              pcm_s16le_planar        y41p
h264_vulkan             pcm_s24be               yuv4
hap                     pcm_s24daud             zlib
hdr                     pcm_s24le               zmbv
hevc_amf                pcm_s24le_planar

Enabled hwaccels:
apv_vulkan              hevc_d3d12va            vc1_d3d12va
av1_d3d11va             hevc_dxva2              vc1_dxva2
av1_d3d11va2            hevc_nvdec              vc1_nvdec
av1_d3d12va             hevc_vaapi              vc1_vaapi
av1_dxva2               hevc_vulkan             vp8_nvdec
av1_nvdec               mjpeg_nvdec             vp8_vaapi
av1_vaapi               mjpeg_vaapi             vp9_d3d11va
av1_vulkan              mpeg1_nvdec             vp9_d3d11va2
dpx_vulkan              mpeg2_d3d11va           vp9_d3d12va
ffv1_vulkan             mpeg2_d3d11va2          vp9_dxva2
h263_vaapi              mpeg2_d3d12va           vp9_nvdec
h264_d3d11va            mpeg2_dxva2             vp9_vaapi
h264_d3d11va2           mpeg2_nvdec             vp9_vulkan
h264_d3d12va            mpeg2_vaapi             vvc_vaapi
h264_dxva2              mpeg4_nvdec             wmv3_d3d11va
h264_nvdec              mpeg4_vaapi             wmv3_d3d11va2
h264_vaapi              prores_raw_vulkan       wmv3_d3d12va
h264_vulkan             prores_vulkan           wmv3_dxva2
hevc_d3d11va            vc1_d3d11va             wmv3_nvdec
hevc_d3d11va2           vc1_d3d11va2            wmv3_vaapi

Enabled parsers:
aac                     dvdsub                  mpegaudio
aac_latm                evc                     mpegvideo
ac3                     ffv1                    opus
adx                     flac                    png
ahx                     ftr                     pnm
amr                     g723_1                  prores
apv                     g729                    prores_raw
av1                     gif                     qoi
avs2                    gsm                     rv34
avs3                    h261                    sbc
bmp                     h263                    sipr
cavsvideo               h264                    tak
cook                    hdr                     vc1
cri                     hevc                    vorbis
dca                     ipu                     vp3
dirac                   jpeg2000                vp8
dnxhd                   jpegxl                  vp9
dnxuc                   jpegxs                  vvc
dolby_e                 lcevc                   webp
dpx                     misc4                   xbm
dvaudio                 mjpeg                   xma
dvbsub                  mlp                     xwd
dvd_nav                 mpeg4video

Enabled demuxers:
aa                      ico                     pcm_f64be
aac                     idcin                   pcm_f64le
aax                     idf                     pcm_mulaw
ac3                     iff                     pcm_s16be
ac4                     ifv                     pcm_s16le
ace                     ilbc                    pcm_s24be
acm                     image2                  pcm_s24le
act                     image2_alias_pix        pcm_s32be
adf                     image2_brender_pix      pcm_s32le
adp                     image2pipe              pcm_s8
ads                     image_bmp_pipe          pcm_u16be
adx                     image_cri_pipe          pcm_u16le
aea                     image_dds_pipe          pcm_u24be
afc                     image_dpx_pipe          pcm_u24le
aiff                    image_exr_pipe          pcm_u32be
aix                     image_gem_pipe          pcm_u32le
alp                     image_gif_pipe          pcm_u8
amr                     image_hdr_pipe          pcm_vidc
amrnb                   image_j2k_pipe          pdv
amrwb                   image_jpeg_pipe         pjs
anm                     image_jpegls_pipe       pmp
apac                    image_jpegxl_pipe       pp_bnk
apc                     image_jpegxs_pipe       pva
ape                     image_pam_pipe          pvf
apm                     image_pbm_pipe          qcp
apng                    image_pcx_pipe          qoa
aptx                    image_pfm_pipe          r3d
aptx_hd                 image_pgm_pipe          rawvideo
apv                     image_pgmyuv_pipe       rcwt
aqtitle                 image_pgx_pipe          realtext
argo_asf                image_phm_pipe          redspark
argo_brp                image_photocd_pipe      rka
argo_cvg                image_pictor_pipe       rl2
asf                     image_png_pipe          rm
asf_o                   image_ppm_pipe          roq
ass                     image_psd_pipe          rpl
ast                     image_qdraw_pipe        rsd
au                      image_qoi_pipe          rso
av1                     image_sgi_pipe          rtp
avi                     image_sunrast_pipe      rtsp
avisynth                image_svg_pipe          s337m
avr                     image_tiff_pipe         sami
avs                     image_vbn_pipe          sap
avs2                    image_webp_pipe         sbc
avs3                    image_xbm_pipe          sbg
bethsoftvid             image_xpm_pipe          scc
bfi                     image_xwd_pipe          scd
bfstm                   imf                     sdns
bink                    ingenient               sdp
binka                   ipmovie                 sdr2
bintext                 ipu                     sds
bit                     ircam                   sdx
bitpacked               iss                     segafilm
bmv                     iv8                     ser
boa                     ivf                     sga
bonk                    ivr                     shorten
brstm                   jacosub                 siff
c93                     jpegxl_anim             simbiosis_imx
caf                     jv                      sln
cavsvideo               kux                     smacker
cdg                     kvag                    smjpeg
cdxl                    laf                     smush
cine                    lc3                     sol
codec2                  libgme                  sox
codec2raw               libmodplug              spdif
concat                  libopenmpt              srt
dash                    live_flv                stl
data                    lmlm4                   str
daud                    loas                    subviewer
dcstr                   lrc                     subviewer1
derf                    luodat                  sup
dfa                     lvf                     svag
dfpwm                   lxf                     svs
dhav                    m4v                     swf
dirac                   matroska                tak
dnxhd                   mca                     tedcaptions
dsf                     mcc                     thp
dsicin                  mgsts                   threedostr
dss                     microdvd                tiertexseq
dts                     mjpeg                   tmv
dtshd                   mjpeg_2000              truehd
dv                      mlp                     tta
dvbsub                  mlv                     tty
dvbtxt                  mm                      txd
dvdvideo                mmf                     ty
dxa                     mods                    usm
ea                      moflex                  v210
ea_cdata                mov                     v210x
eac3                    mp3                     vag
epaf                    mpc                     vc1
evc                     mpc8                    vc1t
ffmetadata              mpegps                  vividas
filmstrip               mpegts                  vivo
fits                    mpegtsraw               vmd
flac                    mpegvideo               vobsub
flic                    mpjpeg                  voc
flv                     mpl2                    vpk
fourxm                  mpsub                   vplayer
frm                     msf                     vqf
fsb                     msnwc_tcp               vvc
fwse                    msp                     w64
g722                    mtaf                    wady
g723_1                  mtv                     wav
g726                    musx                    wavarc
g726le                  mv                      wc3
g728                    mvi                     webm_dash_manifest
g729                    mxf                     webp_anim
gdv                     mxg                     webvtt
genh                    nc                      wsaud
gif                     nistsphere              wsd
gsm                     nsp                     wsvqa
gxf                     nsv                     wtv
h261                    nut                     wv
h263                    nuv                     wve
h264                    obu                     xa
hca                     ogg                     xbin
hcom                    oma                     xmd
hevc                    osq                     xmv
hls                     paf                     xvag
hnm                     pcm_alaw                xwma
hxvs                    pcm_f32be               yop
iamf                    pcm_f32le               yuv4mpegpipe

Enabled muxers:
a64                     h263                    pcm_s16le
ac3                     h264                    pcm_s24be
ac4                     hash                    pcm_s24le
adts                    hds                     pcm_s32be
adx                     hevc                    pcm_s32le
aea                     hls                     pcm_s8
aiff                    iamf                    pcm_u16be
alp                     ico                     pcm_u16le
amr                     ilbc                    pcm_u24be
amv                     image2                  pcm_u24le
apm                     image2pipe              pcm_u32be
apng                    ipod                    pcm_u32le
aptx                    ircam                   pcm_u8
aptx_hd                 ismv                    pcm_vidc
apv                     iterm2                  pdv
argo_asf                ivf                     psp
argo_cvg                jacosub                 rawvideo
asf                     kvag                    rcwt
asf_stream              latm                    rm
ass                     lc3                     roq
ast                     lrc                     rso
au                      m4v                     rtp
avi                     matroska                rtp_mpegts
avif                    matroska_audio          rtsp
avm2                    mcc                     sap
avs2                    md5                     sbc
avs3                    microdvd                scc
bit                     mjpeg                   segafilm
caf                     mkvtimestamp_v2         segment
cavsvideo               mlp                     smjpeg
chromaprint             mmf                     smoothstreaming
codec2                  mov                     sox
codec2raw               mp2                     spdif
crc                     mp3                     spx
dash                    mp4                     srt
data                    mpeg1system             stream_segment
daud                    mpeg1vcd                streamhash
dfpwm                   mpeg1video              sup
dirac                   mpeg2dvd                swf
dnxhd                   mpeg2svcd               tee
dts                     mpeg2video              tg2
dv                      mpeg2vob                tgp
eac3                    mpegts                  truehd
evc                     mpjpeg                  tta
f4v                     mxf                     ttml
ffmetadata              mxf_d10                 uncodedframecrc
fifo                    mxf_opatom              vc1
filmstrip               null                    vc1t
fits                    nut                     voc
flac                    obu                     vvc
flv                     oga                     w64
framecrc                ogg                     wav
framehash               ogv                     webm
framemd5                oma                     webm_chunk
g722                    opus                    webm_dash_manifest
g723_1                  pcm_alaw                webp
g726                    pcm_f32be               webvtt
g726le                  pcm_f32le               whip
gif                     pcm_f64be               wsaud
gsm                     pcm_f64le               wtv
gxf                     pcm_mulaw               wv
h261                    pcm_s16be               yuv4mpegpipe

Enabled protocols:
async                   http                    rtmp
bluray                  httpproxy               rtmpe
cache                   https                   rtmps
concat                  icecast                 rtmpt
concatf                 ipfs_gateway            rtmpte
crypto                  ipns_gateway            rtmpts
data                    librist                 rtp
dtls                    libsrt                  srtp
fd                      libssh                  subfile
ffrtmpcrypt             libzmq                  tcp
ffrtmphttp              md5                     tee
file                    mmsh                    tls
ftp                     mmst                    udp
gopher                  pipe                    udplite
gophers                 prompeg

Enabled filters:
a3dscope                deconvolve              perms
aap                     dedot                   perspective
abench                  deesser                 phase
abitscope               deflate                 photosensitivity
acompressor             deflicker               pixdesctest
acontrast               deinterlace_d3d12       pixelize
acopy                   deinterlace_qsv         pixscope
acrossfade              deinterlace_vaapi       pp7
acrossover              dejudder                premultiply
acrusher                delogo                  premultiply_dynamic
acue                    denoise_vaapi           prewitt
addroi                  deshake                 prewitt_opencl
adeclick                deshake_opencl          procamp_vaapi
adeclip                 despill                 program_opencl
adecorrelate            detelecine              pseudocolor
adelay                  dialoguenhance          psnr
adenorm                 dilation                pullup
aderivative             dilation_opencl         qp
adrawgraph              displace                qrencode
adrc                    doubleweave             qrencodesrc
adynamicequalizer       drawbox                 quirc
adynamicsmooth          drawbox_vaapi           random
aecho                   drawgraph               readeia608
aemphasis               drawgrid                readvitc
aeval                   drawtext                realtime
aevalsrc                drawvg                  remap
aexciter                drmeter                 remap_opencl
afade                   dynaudnorm              removegrain
afdelaysrc              earwax                  removelogo
afftdn                  ebur128                 repeatfields
afftfilt                edgedetect              replaygain
afir                    elbg                    reverse
afireqsrc               entropy                 rgbashift
afirsrc                 epx                     rgbtestsrc
aformat                 eq                      roberts
afreqshift              equalizer               roberts_opencl
afwtdn                  erosion                 rotate
agate                   erosion_opencl          rubberband
agraphmonitor           estdif                  sab
ahistogram              exposure                scale
aiir                    extractplanes           scale2ref
aintegral               extrastereo             scale_cuda
ainterleave             fade                    scale_d3d11
alatency                feedback                scale_d3d12
alimiter                fftdnoiz                scale_qsv
allpass                 fftfilt                 scale_vaapi
allrgb                  field                   scale_vulkan
allyuv                  fieldhint               scdet
aloop                   fieldmatch              scdet_vulkan
alphaextract            fieldorder              scharr
alphamerge              fillborders             scroll
amerge                  find_rect               segment
ametadata               firequalizer            select
amf_capture             flanger                 selectivecolor
amix                    flip_vulkan             sendcmd
amovie                  flite                   separatefields
amplify                 floodfill               setdar
amultiply               format                  setfield
anequalizer             fps                     setparams
anlmdn                  framepack               setpts
anlmf                   framerate               setrange
anlms                   framestep               setsar
anoisesrc               frc_amf                 settb
anull                   freezedetect            sharpness_vaapi
anullsink               freezeframes            shear
anullsrc                frei0r                  showcqt
apad                    frei0r_src              showcwt
aperms                  fspp                    showfreqs
aphasemeter             fsync                   showinfo
aphaser                 gblur                   showpalette
aphaseshift             gblur_vulkan            showspatial
apsnr                   geq                     showspectrum
apsyclip                gfxcapture              showspectrumpic
apulsator               gradfun                 showvolume
arealtime               gradients               showwaves
aresample               graphmonitor            showwavespic
areverse                grayworld               shuffleframes
arls                    greyedge                shufflepixels
arnndn                  guided                  shuffleplanes
asdr                    haas                    sidechaincompress
asegment                haldclut                sidechaingate
aselect                 haldclutsrc             sidedata
asendcmd                hdcd                    sierpinski
asetnsamples            headphone               signalstats
asetpts                 hflip                   signature
asetrate                hflip_vulkan            silencedetect
asettb                  highpass                silenceremove
ashowinfo               highshelf               sinc
asidedata               hilbert                 sine
asisdr                  histeq                  siti
asoftclip               histogram               smartblur
aspectralstats          hqdn3d                  smptebars
asplit                  hqx                     smptehdbars
ass                     hstack                  sobel
astats                  hstack_qsv              sobel_opencl
astreamselect           hstack_vaapi            sofalizer
asubboost               hsvhold                 spectrumsynth
asubcut                 hsvkey                  speechnorm
asupercut               hue                     split
asuperpass              huesaturation           spp
asuperstop              hwdownload              sr_amf
atadenoise              hwmap                   ssim
atempo                  hwupload                ssim360
atilt                   hwupload_cuda           stereo3d
atrim                   hysteresis              stereotools
avectorscope            iccdetect               stereowiden
avgblur                 iccgen                  streamselect
avgblur_opencl          identity                subtitles
avgblur_vulkan          idet                    super2xsai
avsynctest              il                      superequalizer
axcorrelate             inflate                 surround
azmq                    interlace               swaprect
backgroundkey           interlace_vulkan        swapuv
bandpass                interleave              tblend
bandreject              join                    telecine
bass                    kerndeint               testsrc
bbox                    kirsch                  testsrc2
bench                   ladspa                  thistogram
bilateral               lagfun                  threshold
bilateral_cuda          latency                 thumbnail
biquad                  lenscorrection          thumbnail_cuda
bitplanenoise           lensfun                 tile
blackdetect             libplacebo              tiltandshift
blackdetect_vulkan      libvmaf                 tiltshelf
blackframe              life                    tinterlace
blend                   limitdiff               tlut2
blend_vulkan            limiter                 tmedian
blockdetect             loop                    tmidequalizer
blurdetect              loudnorm                tmix
bm3d                    lowpass                 tonemap
boxblur                 lowshelf                tonemap_opencl
boxblur_opencl          lumakey                 tonemap_vaapi
bs2b                    lut                     tpad
bwdif                   lut1d                   transpose
bwdif_cuda              lut2                    transpose_cuda
bwdif_vulkan            lut3d                   transpose_opencl
cas                     lutrgb                  transpose_vaapi
ccrepack                lutyuv                  transpose_vulkan
cellauto                mandelbrot              treble
channelmap              maskedclamp             tremolo
channelsplit            maskedmax               trim
chorus                  maskedmerge             unpremultiply
chromaber_vulkan        maskedmin               unsharp
chromahold              maskedthreshold         unsharp_opencl
chromakey               maskfun                 untile
chromakey_cuda          mcdeint                 uspp
chromanr                mcompand                v360
chromashift             median                  v360_vulkan
ciescope                mergeplanes             vaguedenoiser
codecview               mestimate               varblur
color                   mestimate_d3d12         vectorscope
color_vulkan            metadata                vflip
colorbalance            midequalizer            vflip_vulkan
colorchannelmixer       minterpolate            vfrdet
colorchart              mix                     vibrance
colorcontrast           monochrome              vibrato
colorcorrect            morpho                  vidstabdetect
colordetect             movie                   vidstabtransform
colorhold               mpdecimate              vif
colorize                mptestsrc               vignette
colorkey                msad                    virtualbass
colorkey_opencl         multiply                vmafmotion
colorlevels             negate                  volume
colormap                nlmeans                 volumedetect
colormatrix             nlmeans_opencl          vpp_amf
colorspace              nlmeans_vulkan          vpp_qsv
colorspace_cuda         nnedi                   vstack
colorspectrum           noformat                vstack_qsv
colortemperature        noise                   vstack_vaapi
compand                 normalize               w3fdif
compensationdelay       null                    waveform
concat                  nullsink                weave
convolution             nullsrc                 whisper
convolution_opencl      openclsrc               xbr
convolve                oscilloscope            xcorrelate
copy                    overlay                 xfade
corr                    overlay_cuda            xfade_opencl
cover_rect              overlay_opencl          xfade_vulkan
crop                    overlay_qsv             xmedian
cropdetect              overlay_vaapi           xpsnr
crossfeed               overlay_vulkan          xstack
crystalizer             owdenoise               xstack_qsv
cue                     pad                     xstack_vaapi
curves                  pad_cuda                yadif
datascope               pad_opencl              yadif_cuda
dblur                   pad_vaapi               yaepblur
dcshift                 pal100bars              yuvtestsrc
dctdnoiz                pal75bars               zmq
ddagrab                 palettegen              zoneplate
deband                  paletteuse              zoompan
deblock                 pan                     zscale
decimate                perlin

Enabled bsfs:
aac_adtstoasc           filter_units            opus_metadata
ahx_to_mp2              h264_metadata           pcm_rechunk
apv_metadata            h264_mp4toannexb        pgs_frame_merge
av1_frame_merge         h264_redundant_pps      prores_metadata
av1_frame_split         hapqa_extract           remove_extradata
av1_metadata            hevc_metadata           setts
chomp                   hevc_mp4toannexb        showinfo
dca_core                imx_dump_header         smpte436m_to_eia608
dovi_rpu                lcevc_metadata          text2movsub
dovi_split              media100_to_mjpegb      trace_headers
dts2pts                 mjpeg2jpeg              truehd_core
dump_extradata          mjpega_dump_header      vp9_metadata
dv_error_marker         mov2textsub             vp9_raw_reorder
eac3_core               mpeg2_metadata          vp9_superframe
eia608_to_smpte436m     mpeg4_unpack_bframes    vp9_superframe_split
evc_frame_merge         noise                   vvc_metadata
extract_extradata       null                    vvc_mp4toannexb

Enabled indevs:
dshow                   lavfi                   openal
gdigrab                 libcdio                 vfwcap

Enabled outdevs:
caca

git-full external libraries' versions: 

AMF v1.5.2-1-g6ec0295
aom v3.14.1-108-g43f2b6a990
aribcaption 1.1.1
AviSynthPlus v3.7.5-337-gfcb9c8a2
bs2b 3.1.0
cairo 1.18.5
chromaprint 1.6.0
codec2 1.2.0-108-g310777b1
dav1d 1.5.3-68-gc150ba6c
davs2 1.7-1-gb41cf11
dvdnav 7.0.0-4-g9c5f227
dvdread 7.0.1-91-g980143e
ffnvcodec n13.0.19.0-6-g15ee327
flite v2.2-55-g6c9f20d
frei0r v3.2.3
gsm 1.0.24
ladspa-sdk 1.17
lame 3.100
lc3 1.1.3
lcms2 2.16
lensfun v0.3.95-1986-g698a39ee
libcdio-paranoia 10.2
libgme 0.6.6
libilbc v3.0.4-346-g6adb26d4a4
libjxl v0.12-snapshot
libopencore-amrnb 0.1.6
libopencore-amrwb 0.1.6
libplacebo v7.360.0-99-g05ac2cc
libsoxr 0.1.3
libssh 0.12.0
libtheora v1.2.0
libwebp v1.6.0-195-g733c91e
openal-soft latest
openapv v0.3.0.0
openmpt libopenmpt-0.6.28-35-gf8bd564e
opus v1.6.1-50-g3da9f7a6
qrencode 4.1.1
quirc 1.2
rav1e p20250624-3-g564ae3b
rist 0.2.19
rubberband v1.8.1
SDL release-2.32.0-220-gde433838a
shaderc v2026.2-25-g49a8724
shine 3.1.1
snappy 1.2.2
speex Speex-1.2.1-51-g0589522
srt v1.5.5-9-gc39196c
SVT-AV1 v4.1.0-cqp-extended-126-gd3c4cb394
SVT-JPEG-XS v0.9.0-60-gd2ea183
twolame 0.4.0
uavs3d v1.1-50-g0e20d2c
VAAPI 2.25.0.
vidstab v1.1.1-24-g92bc0b0
vmaf v3.2.0-3-g0f9912e4
vo-amrwbenc 0.1.3
vorbis v1.3.7-36-ge3c9861f
VPL 2.17
vpx v1.16.0-156-g8592391cd
vulkan-loader v1.4.356
vvenc v1.14.0-153-g7d60406
whisper.cpp 1.9.1
x264 v0.165.3223
x265 4.2-65-g4455762
xavs2 1.4
xevd 0.5.0
xeve 0.5.1
xvid v1.3.7
zeromq 4.3.5
zimg release-3.0.6-222-gb364757
zvbi v0.2.44-4-g41477c9

