#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCAsmBackend.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/MC/MCParser/MCAsmParser.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCParser/MCTargetAsmParser.h>
#include <llvm/MC/MCObjectWriter.h>
#include <llvm/MC/MCCodeEmitter.h>

#include <string>
#include <sstream>
#include <assert.h>
#include <iostream>

std::string code = R"(
.text
.global memcpy_x4_kernel_gfx1030
.p2align 8
.type memcpy_x4_kernel_gfx1030,@function
memcpy_x4_kernel_gfx1030:
; This is just an example, not the optimal one
.set s_karg,            0   ; kernel argument
.set s_bx,              2   ; blockIdx

.set s_ptr_in,          4
.set s_ptr_out,         6
.set s_loops_per_block, 8
.set s_stride_block,    10
.set s_tmp,             12
.set s_gdx,             16

.set v_buf,             0
.set v_offset,          16
.set v_tmp,             32

    ; http://www.hsafoundation.com/html/Content/Runtime/Topics/02_Core/hsa_kernel_dispatch_packet_t.htm
    ;s_load_dword s[s_gdx],                  s[s_dptr:s_dptr+1], 12
    ;s_waitcnt           lgkmcnt(0)
    ;s_lshr_b32      s[s_gdx],   s[s_gdx],   8
    ;s_mov_b32   s[s_gdx], 72    ; num_cu

    s_load_dwordx2 s[s_ptr_in:s_ptr_in+1],      s[s_karg:s_karg+1],     0
    s_load_dwordx2 s[s_ptr_out:s_ptr_out+1],    s[s_karg:s_karg+1],     8
    s_load_dword s[s_loops_per_block],          s[s_karg:s_karg+1],     16
    s_load_dword s[s_gdx],                      s[s_karg:s_karg+1],     20

    s_mul_i32 s[s_tmp+1], s[s_bx], 256*4    ; blockIdx*blockDim*4
    v_lshlrev_b32 v[v_tmp], 2, v0           ; threadIdx*4
    v_add_nc_u32 v[v_offset+0], s[s_tmp+1], v[v_tmp]    ; (blockIdx*blockDim + threadIdx)*4
    v_lshlrev_b32 v[v_offset+0], 2, v[v_offset+0]    

    s_waitcnt           lgkmcnt(0)

    s_mul_i32 s[s_tmp],  s[s_gdx],  256*4*4   ; gridDim*blockDim*float4
    v_add_nc_u32 v[v_offset+1],    s[s_tmp],   v[v_offset+0]
    v_add_nc_u32 v[v_offset+2],    s[s_tmp],   v[v_offset+1]
    v_add_nc_u32 v[v_offset+3],    s[s_tmp],   v[v_offset+2]

    s_mul_i32 s[s_tmp],  s[s_gdx],  256*4   ; gridDim*blockDim*4
    s_lshl_b32 s[s_stride_block],   s[s_tmp],   4   ; unroll 16, gridDim*blockDim*4*workload

label_memcopy_start:
    global_load_dwordx4 v[v_buf+0 :v_buf+3],    v[v_offset+0],  s[s_ptr_in:s_ptr_in+1]
    global_load_dwordx4 v[v_buf+4 :v_buf+7],    v[v_offset+1],  s[s_ptr_in:s_ptr_in+1]
    global_load_dwordx4 v[v_buf+8 :v_buf+11],   v[v_offset+2],  s[s_ptr_in:s_ptr_in+1]
    global_load_dwordx4 v[v_buf+12:v_buf+15],   v[v_offset+3],  s[s_ptr_in:s_ptr_in+1]

    s_add_u32   s[s_ptr_in],   s[s_stride_block], s[s_ptr_in]
    s_addc_u32  s[s_ptr_in+1], s[s_ptr_in+1], 0

    s_waitcnt       vmcnt(0)

    global_store_dwordx4    v[v_offset+0],  v[v_buf+0 :v_buf+3],    s[s_ptr_out:s_ptr_out+1]
    global_store_dwordx4    v[v_offset+1],  v[v_buf+4 :v_buf+7],    s[s_ptr_out:s_ptr_out+1]
    global_store_dwordx4    v[v_offset+2],  v[v_buf+8 :v_buf+11],   s[s_ptr_out:s_ptr_out+1]
    global_store_dwordx4    v[v_offset+3],  v[v_buf+12:v_buf+15],   s[s_ptr_out:s_ptr_out+1]

    s_add_u32   s[s_ptr_out],   s[s_stride_block], s[s_ptr_out]
    s_addc_u32  s[s_ptr_out+1], s[s_ptr_out+1], 0

    s_sub_u32 s[s_loops_per_block], s[s_loops_per_block], 1
    s_cmp_eq_u32 s[s_loops_per_block], 0
    s_waitcnt       vmcnt(0)
    s_cbranch_scc0  label_memcopy_start
    s_endpgm

.rodata
.p2align 6
.amdhsa_kernel memcpy_x4_kernel_gfx1030
    .amdhsa_group_segment_fixed_size 0
    .amdhsa_user_sgpr_dispatch_ptr 0
    .amdhsa_user_sgpr_kernarg_segment_ptr 1
    .amdhsa_system_sgpr_workgroup_id_x 1
    .amdhsa_system_vgpr_workitem_id 0
    .amdhsa_next_free_vgpr 64
    .amdhsa_next_free_sgpr 32
    .amdhsa_ieee_mode 0
    .amdhsa_dx10_clamp 0
    .amdhsa_wavefront_size32 1
    .amdhsa_workgroup_processor_mode 0
.end_amdhsa_kernel

.amdgpu_metadata
---
amdhsa.version: [ 1, 0 ]
amdhsa.kernels:
  - .name: memcpy_x4_kernel_gfx1030
    .symbol: memcpy_x4_kernel_gfx1030.kd
    .sgpr_count: 32
    .vgpr_count: 64
    .kernarg_segment_align: 8
    .kernarg_segment_size: 24
    .group_segment_fixed_size: 0
    .private_segment_fixed_size: 0
    .wavefront_size: 32
    .reqd_workgroup_size : [256, 1, 1]
    .max_flat_workgroup_size: 256
    .args:
    - { .name: input,           .size: 8, .offset:   0, .value_kind: global_buffer, .value_type: f32, .address_space: global, .is_const: false}
    - { .name: output,          .size: 8, .offset:   8, .value_kind: global_buffer, .value_type: f32, .address_space: global, .is_const: false}
    - { .name: loops_per_block, .size: 4, .offset:  16, .value_kind: by_value, .value_type: i32}
    - { .name: cu_number,       .size: 4, .offset:  20, .value_kind: by_value, .value_type: i32}
...
.end_amdgpu_metadata

)";

const std::string target_triple = "amdgcn-unknown-amdhsa";  // amdgcn--amdhsa
const std::vector<std::string> llvm_args = {"--amdhsa-code-object-version=4"};
const std::string target_cpu = "gfx1030";
const std::string target_features = "";

int main()
{
    // cc1as_main.cpp
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();

    if (!llvm_args.empty()) {
        unsigned NumArgs = llvm_args.size();
        auto Args = std::make_unique<const char*[]>(NumArgs + 2);
        Args[0] = "clang (LLVM option parsing)";
        for (unsigned i = 0; i != NumArgs; ++i)
            Args[i + 1] = llvm_args[i].c_str();
        Args[NumArgs + 1] = nullptr;
        llvm::cl::ParseCommandLineOptions(NumArgs + 1, Args.get());
    }

    std::string Error;
    const llvm::Target *TheTarget = llvm::TargetRegistry::lookupTarget(target_triple, Error);
    if (!TheTarget){
        std::cout<<"Fail to loopup target:"<<target_triple<<", error:"<<Error;
        return -1;
    }



    std::unique_ptr<llvm::MCRegisterInfo> MRI(TheTarget->createMCRegInfo(target_triple));
    assert(MRI && "Unable to create target register info!");


    llvm::MCTargetOptions MCOptions;
    std::unique_ptr<llvm::MCAsmInfo> MAI(
        TheTarget->createMCAsmInfo(*MRI, target_triple, MCOptions));
    assert(MAI && "Unable to create target asm info!");
    MAI->setRelaxELFRelocations(true);



    std::unique_ptr<llvm::MCSubtargetInfo> STI(
      TheTarget->createMCSubtargetInfo(target_triple, target_cpu, target_features));

    std::unique_ptr<llvm::MemoryBuffer> input = llvm::MemoryBuffer::getMemBuffer(code);

    llvm::SourceMgr SrcMgr;

    // Tell SrcMgr about this buffer, which is what the parser will pick up.
    unsigned BufferIndex = SrcMgr.AddNewSourceBuffer(std::move(input), llvm::SMLoc());

    llvm::MCContext Ctx(llvm::Triple(target_triple),  MAI.get(), MRI.get(), STI.get(), &SrcMgr, &MCOptions);
    bool PIC = false;
    std::unique_ptr<llvm::MCObjectFileInfo> MOFI(
        TheTarget->createMCObjectFileInfo(Ctx, PIC));
    Ctx.setObjectFileInfo(MOFI.get());

    std::error_code EC;
    std::unique_ptr<llvm::raw_fd_ostream> FDOS = std::make_unique<llvm::raw_fd_ostream>("_tmp.o", EC, llvm::sys::fs::OF_None);
    if(EC){
        std::cout<<"_tmp.o fd make fail:"<<EC;
        return -1;
    }

    std::unique_ptr<llvm::MCStreamer> Str;

    std::unique_ptr<llvm::MCInstrInfo> MCII(TheTarget->createMCInstrInfo());
    assert(MCII && "Unable to create instruction info!");


    std::unique_ptr<llvm::MCCodeEmitter> CE(
        TheTarget->createMCCodeEmitter(*MCII, *MRI, Ctx));
    std::unique_ptr<llvm::MCAsmBackend> MAB(
        TheTarget->createMCAsmBackend(*STI, *MRI, MCOptions));
    assert(MAB && "Unable to create asm backend!");

    std::unique_ptr<llvm::MCObjectWriter> OW = MAB->createObjectWriter(*FDOS.get());

    Str.reset(TheTarget->createMCObjectStreamer(
        llvm::Triple(target_triple), Ctx, std::move(MAB), std::move(OW), std::move(CE), *STI,
        0,0,
        /*DWARFMustBeAtTheEnd*/ true));
   //  Str.get()->initSections(0, *STI);

    // Assembly to object compilation should leverage assembly info.
    Str->setUseAssemblerInfoForParsing(true);

    std::unique_ptr<llvm::MCAsmParser> Parser(
      llvm::createMCAsmParser(SrcMgr, Ctx, *Str.get(), *MAI));

    // FIXME: init MCTargetOptions from sanitizer flags here.
    std::unique_ptr<llvm::MCTargetAsmParser> TAP(
      TheTarget->createMCAsmParser(*STI, *Parser, *MCII, MCOptions));

    Parser->setTargetParser(*TAP.get());
    bool Failed = Parser->Run(0);

    std::cout<<"finish, "<<(Failed ? "failed":"successed")<<std::endl;

    // finaly need do: 

}