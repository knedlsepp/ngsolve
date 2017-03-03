
/*********************************************************************/
/* File:   periodic.cpp                                              */
/* Author: Christopher Lackner                                       */
/* Date:   Feb. 2017                                                 */
/*********************************************************************/


#include <comp.hpp>

namespace ngcomp {

  PeriodicFESpace :: PeriodicFESpace (shared_ptr<FESpace> aspace, const Flags & flags, shared_ptr<Array<int>> aused_idnrs)
    : FESpace(aspace->GetMeshAccess(), flags), space(aspace), used_idnrs(aused_idnrs)
    {
      type = "Periodic" + space->type;
      for(auto vb : {VOL,BND,BBND})
        {
          evaluator[vb] = space->GetEvaluator(vb);
          flux_evaluator[vb] = space->GetFluxEvaluator(vb);
          integrator[vb] = space->GetIntegrator(vb);
        }
      iscomplex = space->IsComplex();
      // not yet working...
      if (space->LowOrderFESpacePtr() && false)
        {
          auto lo_flags = flags;
          lo_flags.SetFlag("order",1);
          low_order_space = make_shared<PeriodicFESpace>(space->LowOrderFESpacePtr(),lo_flags,used_idnrs);
        }
    }
    
  void PeriodicFESpace :: Update (LocalHeap & lh)
    {      
      space->Update (lh);
      FESpace::Update(lh);
      dofmap.SetSize (space->GetNDof());
      for (int i = 0; i < dofmap.Size(); i++)
	dofmap[i] = i;

      int nid = ma->GetNetgenMesh()->GetIdentifications().GetMaxNr();

      // identifications are 1 based
      for (int idnr = 1; idnr<nid+1; idnr++)
        {
	  if (used_idnrs->Size() && !used_idnrs->Contains(idnr)) continue;
          Array<int> slave_dofnrs;
          Array<int> master_dofnrs;
          for (auto node_type : {NT_VERTEX, NT_EDGE, NT_FACE})
            {
              auto & periodic_nodes = ma->GetPeriodicNodes(node_type, idnr);
              for(auto node_pair : periodic_nodes)
                {
                  space->GetDofNrs(NodeId(node_type,node_pair[0]),master_dofnrs);
                  space->GetDofNrs(NodeId(node_type,node_pair[1]),slave_dofnrs);
                  for(auto i : Range(master_dofnrs.Size()))
                    {
                    dofmap[slave_dofnrs[i]] = dofmap[master_dofnrs[i]];
		    DofMapped(slave_dofnrs[i],master_dofnrs[i],idnr);
                    }
                }
            }
        }
      ctofdof.SetSize(dofmap.Size());
      for (auto i : Range(ctofdof.Size()))
        ctofdof[i] = space->GetDofCouplingType(i);
      for (int i = 0; i < dofmap.Size(); i++)
	if (dofmap[i] != i){
          ctofdof[i] = UNUSED_DOF;
	}
    }
    
  FiniteElement& PeriodicFESpace :: GetFE (ElementId ei, Allocator & alloc) const
    {
      auto & fe = space->GetFE(ei,alloc);
      const auto & ngel = ma->GetElement(ei);
      switch (ngel.GetType())
	{
	case ET_TRIG:
	  {
            auto hofe = dynamic_cast<VertexOrientedFE<ET_TRIG>*>(&fe);
            if(hofe)
              hofe->SetVertexNumbers(dofmap[ngel.Vertices()]);
            break;
	  }
	case ET_TET:
	  {
            auto hofe = dynamic_cast<VertexOrientedFE<ET_TET>*>(&fe);
            if(hofe)
              hofe->SetVertexNumbers(dofmap[ngel.Vertices()]);
            break;
	  }
        case ET_SEGM:
          {
            auto hofe = dynamic_cast<VertexOrientedFE<ET_SEGM>*>(&fe);
            if (hofe)
              hofe->SetVertexNumbers(dofmap[ngel.Vertices()]);
            break;
          }
        default:
          throw Exception("ElementType not implemented for PeriodicFESpace::GetFE");
	}
      return fe;
    }


  void PeriodicFESpace :: GetDofNrs(ElementId ei, Array<int> & dnums) const
    {
      space->GetDofNrs(ei,dnums);
      for (int i = 0; i< dnums.Size(); i++)
	dnums[i] = dofmap[dnums[i]];
    }

  QuasiPeriodicFESpace :: QuasiPeriodicFESpace(shared_ptr<FESpace> fespace, const Flags & flags, shared_ptr<Array<int>> aused_idnrs, shared_ptr<Array<Complex>> afactors) :
    PeriodicFESpace(fespace, flags, aused_idnrs), factors(afactors)
  {
    // only complex quasiperiodic space implemented
    iscomplex = true;
  }

  void QuasiPeriodicFESpace :: Update (LocalHeap & lh)
  {
    space->Update(lh);
    dof_factors.SetSize(space->GetNDof());
    for (auto i : Range(dof_factors.Size()))
      dof_factors[i] = Complex(1.0,0.0);
    PeriodicFESpace::Update(lh);
  }
  void QuasiPeriodicFESpace :: VTransformMR (ElementId ei, SliceMatrix<double> mat, TRANSFORM_TYPE tt) const
  {
    throw Exception("Shouldn't get here: QuasiPeriodicFESpace::TransformMR, space should always be complex");
  }

  void QuasiPeriodicFESpace :: VTransformMC (ElementId ei, SliceMatrix<Complex> mat, TRANSFORM_TYPE tt) const
  {
    PeriodicFESpace::VTransformMC(ei, mat, tt);
    Array<int> dofnrs;
    space->GetDofNrs(ei, dofnrs);
    for (int i : Range(dofnrs.Size()))
      {
	if (dofnrs[i] != dofmap[dofnrs[i]])
	  {
	    if (tt & TRANSFORM_MAT_LEFT)
	      mat.Row(i) *= conj(dof_factors[dofnrs[i]]);
	    if (tt & TRANSFORM_MAT_RIGHT)
	      mat.Col(i) *= dof_factors[dofnrs[i]];
	  }
      }
  }

    void QuasiPeriodicFESpace :: VTransformVR (ElementId ei, SliceVector<double> vec, TRANSFORM_TYPE tt) const
  {
    throw Exception("Shouldn't get here: QuasiPeroidicFESpace::TransformVR, space should always be complex");
  }

  void QuasiPeriodicFESpace :: VTransformVC (ElementId ei, SliceVector<Complex> vec, TRANSFORM_TYPE tt) const 
  {
    PeriodicFESpace::VTransformVC(ei, vec, tt);
    Array<int> dofnrs;
    space->GetDofNrs(ei, dofnrs);
    for (int i : Range(dofnrs.Size()))
      {
	if (dofnrs[i] != dofmap[dofnrs[i]])
	  {
	    if (tt == TRANSFORM_RHS)
	      vec[i] *= conj(dof_factors[dofnrs[i]]);
	    else
	      vec[i] *= dof_factors[dofnrs[i]];
	  }
      }
    
  }

  void QuasiPeriodicFESpace :: DofMapped(size_t from, size_t to, size_t idnr)
  {
    dof_factors[from] *= (*factors)[idnr-1];
  }

}