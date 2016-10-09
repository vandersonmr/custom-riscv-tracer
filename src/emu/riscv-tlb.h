//
//  riscv-tlb.h
//

#ifndef riscv_tlb_h
#define riscv_tlb_h

namespace riscv {

	/*
	 * tagged_tlb_entry
	 *
	 * protection domain and address space tagged virtual to physical mapping with page attributes
	 *
	 * tlb[PDID:ASID:VPN] = PPN:PTE.bits:PMA
	 */

	template <typename PARAM>
	struct tagged_tlb_entry
	{
		typedef typename PARAM::UX      UX;              /* address type */

		enum : UX {
			asid_bits =   PARAM::asid_bits,
			ppn_bits =    PARAM::ppn_bits,
			vpn_bits =    (sizeof(UX) << 3) - page_shift,
			pte_bits =    page_shift,
			ppn_limit =   (1ULL<<ppn_bits)-1,
			asid_limit =  (1ULL<<asid_bits)-1,
			vpn_limit =   (1ULL<<vpn_bits)-1
		};

		static_assert(asid_bits + ppn_bits == 32 || asid_bits + ppn_bits == 64 ||
			asid_bits + ppn_bits == 128, "asid_bits + ppn_bits == (32, 64, 128)");

		UX      ppn  : ppn_bits;
		UX      asid : asid_bits;
		UX      vpn  : vpn_bits;
		UX      pteb : pte_bits;
		pdid_t  pdid;
		pma_t   pma;

		tagged_tlb_entry() :
			ppn(ppn_limit), asid(asid_limit), vpn(vpn_limit), pteb(0), pdid(0), pma(0) {}

		tagged_tlb_entry(UX pdid, UX asid, UX vpn, UX pteb, UX ppn) :
			ppn(ppn), asid(asid), vpn(vpn), pteb(pteb), pdid(pdid), pma(0) {}
	};


	/*
	 * tagged_tlb
	 *
	 * protection domain and address space tagged tlb
	 *
	 * tlb[PDID:ASID:VPN] = PPN:PTE.bits:PMA
	 */

	template <const size_t tlb_size, typename PARAM>
	struct tagged_tlb
	{
	    static_assert(ispow2(tlb_size), "tlb_size must be a power of 2");

		typedef typename PARAM::UX      UX;              /* address type */
		typedef tagged_tlb_entry<PARAM> tlb_entry_t;     /* TLB entry type */

		enum : UX {
			size = tlb_size,
			shift = ctz_pow2(size),
			mask = (1ULL << shift) - 1,
			key_size = sizeof(tlb_entry_t)
		};

		// TODO - map TLB to machine address space with user_memory::add_segment
		tlb_entry_t tlb[size];

		tagged_tlb() { flush(); }

		void flush()
		{
			for (size_t i = 0; i < size; i++) {
				tlb[i] = tlb_entry_t();
			}
		}

		void flush(UX asid)
		{
			for (size_t i = 0; i < size; i++) {
				if (tlb[i].asid != asid) continue;
				tlb[i] = tlb_entry_t();
			}
		}

		// lookup TLB entry for the given PDID + ASID + X:12[VA] + 11:0[PTE.bits] -> PPN]
		tlb_entry_t* lookup(UX pdid, UX asid, UX va)
		{
			UX vpn = va >> page_shift;
			size_t i = vpn & mask;
			return tlb[i].pdid == pdid && tlb[i].asid == asid && (tlb[i].vpn == vpn) ?
				tlb + i : nullptr;
		}

		// insert TLB entry for the given PDID + ASID + X:12[VA] + 11:0[PTE.bits] <- PPN]
		void insert(UX pdid, UX asid, UX va, UX pteb, UX ppn)
		{
			UX vpn = va >> page_shift;
			size_t i = vpn & mask;
			// we are implicitly evicting an entry by overwriting it
			tlb[i] = tlb_entry_t(pdid, asid, vpn, pteb, ppn);
		}
	};

	template <const size_t tlb_size> using tagged_tlb_rv32 = tagged_tlb<tlb_size,param_rv32>;
	template <const size_t tlb_size> using tagged_tlb_rv64 = tagged_tlb<tlb_size,param_rv64>;

}

#endif