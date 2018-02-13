// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "default.h"
#include "geometry.h"
#include "buffer.h"
#include "../subdiv/bezier_curve.h"
#include "../subdiv/bspline_curve.h"

namespace embree
{
  /*! basis of a curve */
  enum CurveType
  {
    LINEAR_CURVE,
    BEZIER_CURVE,
    BSPLINE_CURVE
  };

  /* rendering mode of a curve */
  enum CurveSubtype
  {
    ROUND_CURVE,
    FLAT_CURVE
  };

  /*! represents an array of bicubic bezier curves */
  struct NativeCurves : public Geometry
  {
    /*! type of this geometry */
    static const Geometry::Type geom_type = Geometry::BEZIER_CURVES;

  public:
    
    /*! bezier curve construction */
    NativeCurves (Device* device, CurveType type, CurveSubtype subtype);
    
  public:
    void enabling();
    void disabling();
    void setMask(unsigned mask);
    void setNumTimeSteps (unsigned int numTimeSteps);
    void setVertexAttributeCount (unsigned int N);
    void setBuffer(RTCBufferType type, unsigned int slot, RTCFormat format, const Ref<Buffer>& buffer, size_t offset, size_t stride, unsigned int num);
    void* getBuffer(RTCBufferType type, unsigned int slot);
    void updateBuffer(RTCBufferType type, unsigned int slot);
    void preCommit();
    void postCommit();
    bool verify();
    void setTessellationRate(float N);

  public:
    
    /*! returns the number of vertices */
    __forceinline size_t numVertices() const {
      return vertices[0].size();
    }

    /*! returns the number of vertices */
    __forceinline size_t numNativeVertices() const {
      return native_vertices[0].size();
    }
    
    /*! returns the i'th curve */
    __forceinline const unsigned int& curve(size_t i) const {
      return native_curves[i];
    }

    /*! returns the i'th segment */
    __forceinline unsigned int getStartEndBitMask(size_t i) const {
      unsigned int mask = 0;
      if (flags) 
        mask |= (flags[i] & 0x3) << 30;
      return mask;
    }


    /*! returns the i'th curve */
    __forceinline const Curve3fa getCurve(size_t i, size_t itime = 0) const 
    {
      const unsigned int index = curve(i);
      const Vec3fa v0 = vertex(index+0,itime);
      const Vec3fa v1 = vertex(index+1,itime);
      const Vec3fa v2 = vertex(index+2,itime);
      const Vec3fa v3 = vertex(index+3,itime);
      return Curve3fa (v0,v1,v2,v3);
    }
    
    /*! returns i'th vertex of the first time step */
    __forceinline Vec3fa vertex(size_t i) const {
      return native_vertices0[i];
    }
    
    /*! returns i'th radius of the first time step */
    __forceinline float radius(size_t i) const {
      return native_vertices0[i].w;
    }

    /*! returns i'th vertex of itime'th timestep */
    __forceinline Vec3fa vertex(size_t i, size_t itime) const {
      return native_vertices[itime][i];
    }
    
    /*! returns i'th radius of itime'th timestep */
    __forceinline float radius(size_t i, size_t itime) const {
      return native_vertices[itime][i].w;
    }

    /*! gathers the curve starting with i'th vertex of itime'th timestep */
    __forceinline void gather(Vec3fa& p0,
                              Vec3fa& p1,
                              Vec3fa& p2,
                              Vec3fa& p3,
                              size_t i,
                              size_t itime = 0) const
    {
      p0 = vertex(i+0,itime);
      p1 = vertex(i+1,itime);
      p2 = vertex(i+2,itime);
      p3 = vertex(i+3,itime);
    }

    /*! prefetches the curve starting with i'th vertex of itime'th timestep */
    __forceinline void prefetchL1_vertices(size_t i) const
    {
      prefetchL1(native_vertices0.getPtr(i)+0);
      prefetchL1(native_vertices0.getPtr(i)+64);
    }

    /*! prefetches the curve starting with i'th vertex of itime'th timestep */
    __forceinline void prefetchL2_vertices(size_t i) const
    {
      prefetchL2(native_vertices0.getPtr(i)+0);
      prefetchL2(native_vertices0.getPtr(i)+64);
    }  

    __forceinline void gather(Vec3fa& p0,
                              Vec3fa& p1,
                              Vec3fa& p2,
                              Vec3fa& p3,
                              size_t i,
                              float time) const
    {
      float ftime;
      const size_t itime = getTimeSegment(time, fnumTimeSegments, ftime);

      const float t0 = 1.0f - ftime;
      const float t1 = ftime;
      Vec3fa a0,a1,a2,a3;
      gather(a0,a1,a2,a3,i,itime);
      Vec3fa b0,b1,b2,b3;
      gather(b0,b1,b2,b3,i,itime+1);
      p0 = madd(Vec3fa(t0),a0,t1*b0);
      p1 = madd(Vec3fa(t0),a1,t1*b1);
      p2 = madd(Vec3fa(t0),a2,t1*b2);
      p3 = madd(Vec3fa(t0),a3,t1*b3);
    }
    
    /*! calculates bounding box of i'th bezier curve */
    __forceinline BBox3fa bounds(size_t i, size_t itime = 0) const
    {
      const Curve3fa curve = getCurve(i,itime);
      if (likely(subtype == FLAT_CURVE))
        return curve.tessellatedBounds(tessellationRate);
      else
        return curve.accurateBounds();
    }
    
    /*! calculates bounding box of i'th bezier curve */
    __forceinline BBox3fa bounds(const AffineSpace3fa& space, size_t i, size_t itime = 0) const
    {
      const unsigned int index = curve(i);
      const Vec3fa v0 = vertex(index+0,itime);
      const Vec3fa v1 = vertex(index+1,itime);
      const Vec3fa v2 = vertex(index+2,itime);
      const Vec3fa v3 = vertex(index+3,itime);
      Vec3fa w0 = xfmPoint(space,v0); w0.w = v0.w;
      Vec3fa w1 = xfmPoint(space,v1); w1.w = v1.w;
      Vec3fa w2 = xfmPoint(space,v2); w2.w = v2.w;
      Vec3fa w3 = xfmPoint(space,v3); w3.w = v3.w;
      const Curve3fa curve(w0,w1,w2,w3);
      if (likely(subtype == FLAT_CURVE)) return curve.tessellatedBounds(tessellationRate);
      else                               return curve.accurateBounds();
    }

    /*! calculates bounding box of i'th bezier curve */
    __forceinline BBox3fa bounds(const Vec3fa& ofs, const float scale, const float r_scale0, const LinearSpace3fa& space, size_t i, size_t itime = 0) const
    {
      const float r_scale = r_scale0*scale;
      const unsigned int index = curve(i);
      const Vec3fa v0 = vertex(index+0,itime);
      const Vec3fa v1 = vertex(index+1,itime);
      const Vec3fa v2 = vertex(index+2,itime);
      const Vec3fa v3 = vertex(index+3,itime);
      Vec3fa w0 = xfmPoint(space,(v0-ofs)*Vec3fa(scale)); w0.w = v0.w*r_scale;
      Vec3fa w1 = xfmPoint(space,(v1-ofs)*Vec3fa(scale)); w1.w = v1.w*r_scale;
      Vec3fa w2 = xfmPoint(space,(v2-ofs)*Vec3fa(scale)); w2.w = v2.w*r_scale;
      Vec3fa w3 = xfmPoint(space,(v3-ofs)*Vec3fa(scale)); w3.w = v3.w*r_scale;
      const Curve3fa curve(w0,w1,w2,w3);
      if (likely(subtype == FLAT_CURVE)) return curve.tessellatedBounds(tessellationRate);
      else                               return curve.accurateBounds();
    }

    /*! check if the i'th primitive is valid at the itime'th timestep */
    __forceinline bool valid(size_t i, size_t itime) const {
      return valid(i, make_range(itime, itime));
    }

    /*! check if the i'th primitive is valid at the itime'th time step */
    __forceinline bool valid(size_t i, const range<size_t>& itime_range) const
    {
      const unsigned int index = curve(i);
      if (index+3 >= numNativeVertices()) return false;

      for (size_t itime = itime_range.begin(); itime <= itime_range.end(); itime++)
      {
        const float r0 = radius(index+0,itime);
        const float r1 = radius(index+1,itime);
        const float r2 = radius(index+2,itime);
        const float r3 = radius(index+3,itime);
        if (!isvalid(r0) || !isvalid(r1) || !isvalid(r2) || !isvalid(r3))
          return false;
        if (min(r0,r1,r2,r3) < 0.0f)
          return false;
        
        const Vec3fa v0 = vertex(index+0,itime);
        const Vec3fa v1 = vertex(index+1,itime);
        const Vec3fa v2 = vertex(index+2,itime);
        const Vec3fa v3 = vertex(index+3,itime);
        if (!isvalid(v0) || !isvalid(v1) || !isvalid(v2) || !isvalid(v3))
          return false;
      }

      return true;
    }

    /*! calculates the linear bounds of the i'th primitive at the itimeGlobal'th time segment */
    __forceinline LBBox3fa linearBounds(size_t i, size_t itime) const {
      return LBBox3fa(bounds(i,itime+0),bounds(i,itime+1));
    }

    /*! calculates the linear bounds of the i'th primitive at the itimeGlobal'th time segment */
    __forceinline LBBox3fa linearBounds(const AffineSpace3fa& space, size_t i, size_t itime) const {
      return LBBox3fa(bounds(space,i,itime+0),bounds(space,i,itime+1));
    }

    /*! calculates the linear bounds of the i'th primitive for the specified time range */
    __forceinline LBBox3fa linearBounds(size_t primID, const BBox1f& time_range) const {
      return LBBox3fa([&] (size_t itime) { return bounds(primID, itime); }, time_range, fnumTimeSegments);
    }

    /*! calculates the linear bounds of the i'th primitive for the specified time range */
    __forceinline LBBox3fa linearBounds(const AffineSpace3fa& space, size_t primID, const BBox1f& time_range) const {
      return LBBox3fa([&] (size_t itime) { return bounds(space, primID, itime); }, time_range, fnumTimeSegments);
    }

    /*! calculates the linear bounds of the i'th primitive for the specified time range */
    __forceinline LBBox3fa linearBounds(const Vec3fa& ofs, const float scale, const float r_scale0, const LinearSpace3fa& space, size_t primID, const BBox1f& time_range) const {
      return LBBox3fa([&] (size_t itime) { return bounds(ofs, scale, r_scale0, space, primID, itime); }, time_range, fnumTimeSegments);
    }

    /*! calculates the build bounds of the i'th primitive, if it's valid */
    __forceinline bool buildBounds(size_t i, BBox3fa* bbox = nullptr) const
    {
      const unsigned int index = curve(i);
      if (index+3 >= numNativeVertices()) return false;

      for (size_t t=0; t<numTimeSteps; t++)
      {
        const float r0 = radius(index+0,t);
        const float r1 = radius(index+1,t);
        const float r2 = radius(index+2,t);
        const float r3 = radius(index+3,t);
        if (!isvalid(r0) || !isvalid(r1) || !isvalid(r2) || !isvalid(r3))
          return false;
        //if (min(r0,r1,r2,r3) < 0.0f)
        //  return false;

        const Vec3fa v0 = vertex(index+0,t);
        const Vec3fa v1 = vertex(index+1,t);
        const Vec3fa v2 = vertex(index+2,t);
        const Vec3fa v3 = vertex(index+3,t);
        if (!isvalid(v0) || !isvalid(v1) || !isvalid(v2) || !isvalid(v3))
          return false;
      }

      if (bbox) *bbox = bounds(i);
      return true;
    }

    /*! calculates the i'th build primitive at the itime'th time segment, if it's valid */
    __forceinline bool buildPrim(size_t i, size_t itime, Vec3fa& c0, Vec3fa& c1, Vec3fa& c2, Vec3fa& c3) const
    {
      const unsigned int index = curve(i);
      if (index+3 >= numNativeVertices()) return false;
      const Vec3fa a0 = vertex(index+0,itime+0); if (unlikely(!isvalid((vfloat4)a0))) return false;
      const Vec3fa a1 = vertex(index+1,itime+0); if (unlikely(!isvalid((vfloat4)a1))) return false;
      const Vec3fa a2 = vertex(index+2,itime+0); if (unlikely(!isvalid((vfloat4)a2))) return false;
      const Vec3fa a3 = vertex(index+3,itime+0); if (unlikely(!isvalid((vfloat4)a3))) return false;
      const Vec3fa b0 = vertex(index+0,itime+1); if (unlikely(!isvalid((vfloat4)b0))) return false;
      const Vec3fa b1 = vertex(index+1,itime+1); if (unlikely(!isvalid((vfloat4)b1))) return false;
      const Vec3fa b2 = vertex(index+2,itime+1); if (unlikely(!isvalid((vfloat4)b2))) return false;
      const Vec3fa b3 = vertex(index+3,itime+1); if (unlikely(!isvalid((vfloat4)b3))) return false;
      if (unlikely(min(a0.w,a1.w,a2.w,a3.w) < 0.0f)) return false;
      if (unlikely(min(b0.w,b1.w,b2.w,b3.w) < 0.0f)) return false;
      c0 = 0.5f*(a0+b0);
      c1 = 0.5f*(a1+b1);
      c2 = 0.5f*(a2+b2);
      c3 = 0.5f*(a3+b3);
      return true;
    }

    /*! calculates the linear bounds of the i'th primitive for the specified time range */
    __forceinline bool linearBounds(size_t i, const BBox1f& time_range, LBBox3fa& bbox) const  {
      if (!valid(i, getTimeSegmentRange(time_range, fnumTimeSegments))) return false;
      bbox = linearBounds(i, time_range);
      return true;
    }

  public:
    BufferView<unsigned int> curves;        //!< array of curve indices
    vector<BufferView<Vec3fa>> vertices;    //!< vertex array for each timestep
    BufferView<char> flags;                 //!< start, end flag per segment
    vector<BufferView<char>> vertexAttribs; //!< user buffers
    CurveType type;                         //!< basis of user provided vertices
    CurveSubtype subtype;                   //!< round of flat curve
    int tessellationRate;                   //!< tessellation rate for bezier curve
  public:
    BufferView<Vec3fa> native_vertices0;        //!< fast access to first vertex buffer
    BufferView<unsigned int> native_curves;     //!< array of curve indices
    vector<BufferView<Vec3fa>> native_vertices; //!< vertex array for each timestep
  };

  namespace isa
  {
    struct NativeCurvesISA : public NativeCurves
    {
      NativeCurvesISA (Device* device, CurveType type, CurveSubtype subtype)
        : NativeCurves(device,type,subtype) {}

      template<typename Curve> void interpolate_helper(const RTCInterpolateArguments* const args);
      
      template<typename InputCurve3fa, typename OutputCurve3fa> void commit_helper();
    };
    
    struct CurvesBezier : public NativeCurvesISA
    {
      CurvesBezier (Device* device, CurveType type, CurveSubtype subtype)
         : NativeCurvesISA(device,type,subtype) {}

      void preCommit();
      void interpolate(const RTCInterpolateArguments* const args);

      PrimInfo createPrimRefArray(mvector<PrimRef>& prims, const range<size_t>& r, size_t k) const
      {
        PrimInfo pinfo(empty);
        for (size_t j=r.begin(); j<r.end(); j++)
        {
          BBox3fa bounds = empty;
          if (!buildBounds(j,&bounds)) continue;
          const PrimRef prim(bounds,geomID,unsigned(j));
          pinfo.add_center2(prim);
          prims[k++] = prim;
        }
        return pinfo;
      }

      PrimInfoMB createPrimRefMBArray(mvector<PrimRefMB>& prims, const BBox1f& t0t1, const range<size_t>& r, size_t k) const
      {
        PrimInfoMB pinfo(empty);
        for (size_t j=r.begin(); j<r.end(); j++)
        {
          LBBox3fa bounds = empty;
          if (!this->linearBounds(j,t0t1,bounds)) continue;
          const PrimRefMB prim(bounds,this->numTimeSegments(),this->numTimeSegments(),this->geomID,unsigned(j));
          pinfo.add_primref(prim);
          prims[k++] = prim;
        }
        return pinfo;
      }

      LinearSpace3fa computeAlignedSpace(const size_t primID) const
      {
        Vec3fa axisz(0,0,1);
        Vec3fa axisy(0,1,0);
        
        const unsigned vtxID = this->curve(primID);
        const Vec3fa v0 = this->vertex(vtxID+0);
        const Vec3fa v1 = this->vertex(vtxID+1);
        const Vec3fa v2 = this->vertex(vtxID+2);
        const Vec3fa v3 = this->vertex(vtxID+3);
        const Curve3fa curve(v0,v1,v2,v3);
        const Vec3fa p0 = curve.begin();
        const Vec3fa p3 = curve.end();
        const Vec3fa d0 = curve.eval_du(0.0f);
        //const Vec3fa d1 = curve.eval_du(1.0f);
        const Vec3fa axisz_ = normalize(p3 - p0);
        const Vec3fa axisy_ = cross(axisz_,d0);
        if (sqr_length(p3-p0) > 1E-18f) {
          axisz = axisz_;
          axisy = axisy_;
        }
        
        if (sqr_length(axisy) > 1E-18) {
          axisy = normalize(axisy);
          Vec3fa axisx = normalize(cross(axisy,axisz));
          return LinearSpace3fa(axisx,axisy,axisz);
        }
        return frame(axisz);
      }

      LinearSpace3fa computeAlignedSpaceMB(const size_t primID, const BBox1f time_range) const // FIXME: improve
      {
        Vec3fa axis0(0,0,1);

        const unsigned num_time_segments = this->numTimeSegments();
        const range<int> tbounds = getTimeSegmentRange(time_range, (float)num_time_segments);
        if (tbounds.size() == 0) return frame(axis0);
        
        const size_t t = (tbounds.begin()+tbounds.end())/2;
        const unsigned int vertexID = this->curve(primID);
        const Vec3fa a0 = this->vertex(vertexID+0,t);
        const Vec3fa a1 = this->vertex(vertexID+1,t);
        const Vec3fa a2 = this->vertex(vertexID+2,t);
        const Vec3fa a3 = this->vertex(vertexID+3,t);
        const Curve3fa curve(a0,a1,a2,a3);
        const Vec3fa p0 = curve.begin();
        const Vec3fa p3 = curve.end();
        const Vec3fa axis1 = p3 - p0;
        
        if (sqr_length(axis1) > 1E-18f) {
          axis0 = normalize(axis1);
        }
           
        return frame(axis0);
      }
      
      Vec3fa computeDirection(unsigned int primID) const
      {
        const unsigned vtxID = curve(primID);
        const Vec3fa v0 = vertex(vtxID+0);
        const Vec3fa v1 = vertex(vtxID+1);
        const Vec3fa v2 = vertex(vtxID+2);
        const Vec3fa v3 = vertex(vtxID+3);
        const Curve3fa c(v0,v1,v2,v3);
        const Vec3fa p0 = c.begin();
        const Vec3fa p3 = c.end();
        const Vec3fa axis1 = p3 - p0;
        return axis1;
      }

      Vec3fa computeDirection(unsigned int primID, size_t time) const
      {
        const unsigned vtxID = curve(primID);
        const Vec3fa v0 = vertex(vtxID+0,time);
        const Vec3fa v1 = vertex(vtxID+1,time);
        const Vec3fa v2 = vertex(vtxID+2,time);
        const Vec3fa v3 = vertex(vtxID+3,time);
        const Curve3fa c(v0,v1,v2,v3);
        const Vec3fa p0 = c.begin();
        const Vec3fa p3 = c.end();
        const Vec3fa axis1 = p3 - p0;
        return axis1;
      }

      BBox3fa vbounds(size_t i) const {
        return bounds(i);
      }
      
      BBox3fa vbounds(const AffineSpace3fa& space, size_t i) const {
        return bounds(space,i);
      }

      BBox3fa vbounds(const Vec3fa& ofs, const float scale, const float r_scale0, const LinearSpace3fa& space, size_t i, size_t itime = 0) const {
        return bounds(ofs,scale,r_scale0,space,i,itime);
      }

      LBBox3fa vlinearBounds(const AffineSpace3fa& space, size_t primID, const BBox1f& time_range) const {
        return linearBounds(space,primID,time_range);
      }

      LBBox3fa vlinearBounds(const Vec3fa& ofs, const float scale, const float r_scale0, const LinearSpace3fa& space, size_t primID, const BBox1f& time_range) const {
        return linearBounds(ofs,scale,r_scale0,space,primID,time_range);
      }
    };
    
    struct CurvesBSpline : public NativeCurvesISA
    {
      CurvesBSpline (Device* device, CurveType type, CurveSubtype subtype)
         : NativeCurvesISA(device,type,subtype) {}

      void preCommit();
      void interpolate(const RTCInterpolateArguments* const args);

      PrimInfo createPrimRefArray(mvector<PrimRef>& prims, const range<size_t>& r, size_t k) const
      {
        PrimInfo pinfo(empty);
        for (size_t j=r.begin(); j<r.end(); j++)
        {
          BBox3fa bounds = empty;
          if (!buildBounds(j,&bounds)) continue;
          const PrimRef prim(bounds,geomID,unsigned(j));
          pinfo.add_center2(prim);
          prims[k++] = prim;
        }
        return pinfo;
      }

      PrimInfoMB createPrimRefMBArray(mvector<PrimRefMB>& prims, const BBox1f& t0t1, const range<size_t>& r, size_t k) const
      {
        PrimInfoMB pinfo(empty);
        for (size_t j=r.begin(); j<r.end(); j++)
        {
          LBBox3fa bounds = empty;
          if (!this->linearBounds(j,t0t1,bounds)) continue;
          const PrimRefMB prim(bounds,this->numTimeSegments(),this->numTimeSegments(),this->geomID,unsigned(j));
          pinfo.add_primref(prim);
          prims[k++] = prim;
        }
        return pinfo;
      }

      LinearSpace3fa computeAlignedSpace(const size_t primID) const
      {
        Vec3fa axisz(0,0,1);
        Vec3fa axisy(0,1,0);
        
        const unsigned vtxID = this->curve(primID);
        const Vec3fa v0 = this->vertex(vtxID+0);
        const Vec3fa v1 = this->vertex(vtxID+1);
        const Vec3fa v2 = this->vertex(vtxID+2);
        const Vec3fa v3 = this->vertex(vtxID+3);
        const Curve3fa curve(v0,v1,v2,v3);
        const Vec3fa p0 = curve.begin();
        const Vec3fa p3 = curve.end();
        const Vec3fa d0 = curve.eval_du(0.0f);
        //const Vec3fa d1 = curve.eval_du(1.0f);
        const Vec3fa axisz_ = normalize(p3 - p0);
        const Vec3fa axisy_ = cross(axisz_,d0);
        if (sqr_length(p3-p0) > 1E-18f) {
          axisz = axisz_;
          axisy = axisy_;
        }
        
        if (sqr_length(axisy) > 1E-18) {
          axisy = normalize(axisy);
          Vec3fa axisx = normalize(cross(axisy,axisz));
          return LinearSpace3fa(axisx,axisy,axisz);
        }
        return frame(axisz);
      }

      LinearSpace3fa computeAlignedSpaceMB(const size_t primID, const BBox1f time_range) const // FIXME: improve
      {
        Vec3fa axis0(0,0,1);

        const unsigned num_time_segments = this->numTimeSegments();
        const range<int> tbounds = getTimeSegmentRange(time_range, (float)num_time_segments);
        if (tbounds.size() == 0) return frame(axis0);
        
        const size_t t = (tbounds.begin()+tbounds.end())/2;
        const unsigned int vertexID = this->curve(primID);
        const Vec3fa a0 = this->vertex(vertexID+0,t);
        const Vec3fa a1 = this->vertex(vertexID+1,t);
        const Vec3fa a2 = this->vertex(vertexID+2,t);
        const Vec3fa a3 = this->vertex(vertexID+3,t);
        const Curve3fa curve(a0,a1,a2,a3);
        const Vec3fa p0 = curve.begin();
        const Vec3fa p3 = curve.end();
        const Vec3fa axis1 = p3 - p0;
        
        if (sqr_length(axis1) > 1E-18f) {
          axis0 = normalize(axis1);
        }
           
        return frame(axis0);
      }

      Vec3fa computeDirection(unsigned int primID) const
      {
        const unsigned vtxID = curve(primID);
        const Vec3fa v0 = vertex(vtxID+0);
        const Vec3fa v1 = vertex(vtxID+1);
        const Vec3fa v2 = vertex(vtxID+2);
        const Vec3fa v3 = vertex(vtxID+3);
        const Curve3fa c(v0,v1,v2,v3);
        const Vec3fa p0 = c.begin();
        const Vec3fa p3 = c.end();
        const Vec3fa axis1 = p3 - p0;
        return axis1;
      }

      Vec3fa computeDirection(unsigned int primID, size_t time) const
      {
        const unsigned vtxID = curve(primID);
        const Vec3fa v0 = vertex(vtxID+0,time);
        const Vec3fa v1 = vertex(vtxID+1,time);
        const Vec3fa v2 = vertex(vtxID+2,time);
        const Vec3fa v3 = vertex(vtxID+3,time);
        const Curve3fa c(v0,v1,v2,v3);
        const Vec3fa p0 = c.begin();
        const Vec3fa p3 = c.end();
        const Vec3fa axis1 = p3 - p0;
        return axis1;
      }

      BBox3fa vbounds(size_t i) const {
        return bounds(i);
      }
      
      BBox3fa vbounds(const AffineSpace3fa& space, size_t i) const {
        return bounds(space,i);
      }

      BBox3fa vbounds(const Vec3fa& ofs, const float scale, const float r_scale0, const LinearSpace3fa& space, size_t i, size_t itime = 0) const {
        return bounds(ofs,scale,r_scale0,space,i,itime);
      }

      LBBox3fa vlinearBounds(const AffineSpace3fa& space, size_t primID, const BBox1f& time_range) const {
        return linearBounds(space,primID,time_range);
      }

      LBBox3fa vlinearBounds(const Vec3fa& ofs, const float scale, const float r_scale0, const LinearSpace3fa& space, size_t primID, const BBox1f& time_range) const {
        return linearBounds(ofs,scale,r_scale0,space,primID,time_range);
      }
    };
  }

  DECLARE_ISA_FUNCTION(NativeCurves*, createCurvesBezier, Device* COMMA CurveSubtype);
  DECLARE_ISA_FUNCTION(NativeCurves*, createCurvesBSpline, Device* COMMA CurveSubtype);
}
