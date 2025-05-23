/*
 * Copyright (c) 2020 Barcelona Supercomputing Center
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Cristóbal Ramírez
 */

#ifndef __CPU_VECTOR_ENGINE_HH__
#define __CPU_VECTOR_ENGINE_HH__

#include <algorithm>
#include <functional>
#include <numeric>
#include <string>
#include <vector>

#include "arch/generic/tlb.hh"
#include "arch/riscv/insts/vector_static_inst.hh"
#include "base/statistics.hh"
#include "base/types.hh"
#include "cpu/minor/exec_context.hh"
#include "cpu/simple_thread.hh"
#include "cpu/translation.hh"
#include "cpu/vector_engine/packet.hh"
#include "cpu/vector_engine/req_state.hh"
#include "cpu/vector_engine/vector_dyn_inst.hh"
#include "cpu/vector_engine/vmu/vector_mem_unit.hh"
#include "cpu/vector_engine/vpu/vector_config/vector_config.hh"
#include "cpu/vector_engine/vpu/issue_queues/inst_queue.hh"
#include "cpu/vector_engine/vpu/multilane_wrapper/vector_lane.hh"
#include "cpu/vector_engine/vpu/register_file/vector_reg.hh"
#include "cpu/vector_engine/vpu/register_file/vector_reg_valid_bit.hh"
#include "cpu/vector_engine/vpu/rename/vector_rename.hh"
#include "cpu/vector_engine/vpu/rob/reorder_buffer.hh"
#include "mem/request.hh"
#include "params/VectorEngine.hh"
#include "sim/faults.hh"
#include "sim/sim_object.hh"

/**
 * The vector instructions are decoded by the CPU, and then go through each
 * stage until they reach the commit stage, where they are sent to the Vector
 * Engine. This design decision has been made considering the vector
 * architecture as a decoupled machine.
 * Since we must ensure that the vector instruction execution won’t be
 * interrupted by any possible exception generated by older scalar instructions
 * the core sent at commit stage the vector instruction.
 * This guarantees that the vector instruction will be the older one.
 * When the vector instructions arrive to the Vector Engine, they are first
 * renamed. Then, the instruction and its related data are allocated in
 * temporal queues depending on the instruction type: arithmetic or memory.
 * Once allocated in the corresponding queue, the instruction waits until it
 * fulfills the requirements to be issued:
 * its operands become ready and the corresponding execution unit is available
 * (the memory unit or the vector lane). Finally, when the instruction
 * completes execution, the commit is done by retiring the instruction and
 * freeing up the hardware resources consumed.
 */
class VectorEngine : public SimObject
{
public:
    class VectorMemPort : public MasterPort
    {
      public:
        VectorMemPort(const std::string& name, VectorEngine* owner,
            uint8_t channels);
        ~VectorMemPort();

        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;

        bool startTranslation(Addr addr, uint8_t *data, uint64_t size,
            BaseTLB::Mode mode, ThreadContext *tc, uint64_t req_id,
            uint8_t channel);
        bool sendTimingReadReq(Addr addr, uint64_t size, ThreadContext *tc,
            uint64_t req_id, uint8_t channel);
        bool sendTimingWriteReq(Addr addr, uint8_t *data, uint64_t size,
            ThreadContext *tc, uint64_t req_id, uint8_t channel);

        std::vector< std::deque<PacketPtr> > laCachePktQs;
        VectorEngine *owner;

        class Tlb_Translation : public BaseTLB::Translation
        {
          public:
            Tlb_Translation(VectorEngine *owner);
            ~Tlb_Translation();

            void markDelayed() override;
            /** TLB interace */
            void finish(const Fault &_fault,const RequestPtr &_req,
                ThreadContext *_tc, BaseTLB::Mode _mode) ;

            void finish(const Fault _fault, uint64_t latency);
            std::string name();
          private:
            void translated();
            EventWrapper<Tlb_Translation,&Tlb_Translation::translated> event;
            VectorEngine *owner;
          public:
            Fault fault;
        };
    };

    class VectorRegPort : public MasterPort
    {
    public:
        VectorRegPort(const std::string& name, VectorEngine* owner,
            uint64_t channel);
        ~VectorRegPort();

        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;

        bool sendTimingReadReq(Addr addr, uint64_t size,
            uint64_t req_id);
        bool sendTimingWriteReq(Addr addr, uint8_t *data, uint64_t size,
            uint64_t req_id);

        VectorEngine *owner;
        const uint64_t channel;
    };
public:
    VectorEngine(VectorEngineParams *p);
    ~VectorEngine();

    VectorConfig  *   vector_config;
    //used to identify ports uniquely to whole memory system
    MasterID VectorCacheMasterId;
    VectorMemPort vectormem_port;
    //used to identify ports uniquely to whole memory system
    std::vector<MasterID> VectorRegMasterIds;
    std::vector<VectorRegPort> VectorRegPorts;

    VectorRegister * vector_reg;
    //MasterPort &getMasterPort(const std::string &if_name,
    //                             PortID idx = InvalidPortID)/* override*/;
    Port& getPort(const std::string& if_name,
                                  PortID idx = InvalidPortID) override;
    MasterPort &getVectorMemPort() { return vectormem_port; }

    void recvTimingResp(VectorPacketPtr pkt);

    bool writeVectorReg(Addr addr, uint8_t *data, uint32_t size,
        uint8_t channel,
        std::function<void(void)> callback);
    bool readVectorReg(Addr addr, uint32_t size,
        uint8_t channel,
        std::function<void(uint8_t*,uint8_t)> callback);

    bool writeVectorMem(Addr addr, uint8_t *data, uint32_t size,
        ThreadContext *tc, uint8_t channel,
        std::function<void(void)> callback);
    bool readVectorMem(Addr addr, uint32_t size, ThreadContext *tc,
        uint8_t channel,
        std::function<void(uint8_t*,uint8_t)> callback);

    //fields for keeping track of outstanding requests for reordering
    uint64_t uniqueReqId;
    std::deque<Vector_ReqState *> vector_PendingReqQ;


    bool requestGrant(RiscvISA::VectorStaticInst* insn);
    bool isOccupied();
    bool cluster_available();

    void dispatch(RiscvISA::VectorStaticInst& insn ,ExecContextPtr& xc ,
        uint64_t src1, uint64_t src2, std::function<void()> dependencie_callback);
    void renameVectorInst(RiscvISA::VectorStaticInst& insn, VectorDynInst *dyn_insn);

    void issue(RiscvISA::VectorStaticInst& insn, VectorDynInst *dyn_insn,
        ExecContextPtr& xc,
    uint64_t src1 , uint64_t src2,uint64_t vtype,uint64_t vl,
        std::function<void(Fault fault)> done_callback);

    void regStats() override;

    void printConfigInst(RiscvISA::VectorStaticInst& insn,uint64_t src1,uint64_t src2);
    void printMemInst(RiscvISA::VectorStaticInst& insn,VectorDynInst *vector_dyn_insn);
    void printArithInst(RiscvISA::VectorStaticInst& insn,VectorDynInst *vector_dyn_insn);

public:
    bool masked_op;
    bool vx_op;
    bool vf_op;
    bool vi_op;

    bool dst_write_ena;

    uint8_t num_clusters;
    uint8_t num_lanes;

    ReorderBuffer *   vector_rob;
    std::vector<VectorLane *>  vector_lane;
    VectorMemUnit *   vector_memory_unit;
    InstQueue     *   vector_inst_queue;
    VectorRename  *   vector_rename;
    VectorValidBit *  vector_reg_validbit;

public:
    // Stat for number of Vector Arithmetic Instructions
    Stats::Scalar VectorArithmeticIns;
    // Stat for number of Vector Memory Instructions
    Stats::Scalar VectorMemIns;
    // Stat for number of Vector Set Instructions
    Stats::Scalar VectorConfigIns;
    // Stat for number of Vector Operations (1 vector inst = VL operations)
    Stats::Scalar VectorOp;

    // Stat for Average VL)
    Stats::Scalar TotalVL;
    // Stat for Average VL)
    Stats::Scalar SumVL;
    // Stat for Average VL)
    Stats::Formula AverageVL;
private:
    uint64_t last_vtype;
    uint64_t last_vl;

    /* lmul parameter */
    uint8_t last_lmul=1;
    /* Physical registers */
    uint64_t PDst;
    uint64_t POldDst;
    uint64_t Pvs1,Pvs2,Pvs3;
    uint64_t PMask;
private:
    static int s_VLENB;
public:
    static int getVlenb();
};

#endif // __CPU_VECTOR_ENGINE_HH__
