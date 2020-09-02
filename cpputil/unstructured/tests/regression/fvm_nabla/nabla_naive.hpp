#pragma once

#include "gridtools/next/iteration_space.hpp"
#include "gridtools/next/mesh.hpp"
#include "gridtools/sid/concept.hpp"
struct connectivity_tag;
struct S_MXX_tag;
struct S_MYY_tag;
struct zavgS_MXX_tag;
struct zavgS_MYY_tag;

struct pnabla_MXX_tag;
struct pnabla_MYY_tag;
struct vol_tag;
struct sign_tag;

template <class Mesh,
    class S_MXX_t,
    class S_MYY_t,
    class zavgS_MXX_t,
    class zavgS_MYY_t,
    class pp_t,
    class pnabla_MXX_t,
    class pnabla_MYY_t,
    class vol_t,
    class sign_t>
void nabla(Mesh &&mesh,
    S_MXX_t &&S_MXX,
    S_MYY_t &&S_MYY,
    zavgS_MXX_t &&zavgS_MXX,
    zavgS_MYY_t &&zavgS_MYY,
    pp_t &&pp,
    pnabla_MXX_t &&pnabla_MXX,
    pnabla_MYY_t &&pnabla_MYY,
    vol_t &&vol,
    sign_t &&sign) {
    namespace tu = gridtools::tuple_util;
    namespace gtsh = gridtools::next::sid_helper;

    static_assert(gridtools::is_sid<S_MXX_t>{});
    static_assert(gridtools::is_sid<S_MYY_t>{});
    static_assert(gridtools::is_sid<zavgS_MXX_t>{});
    static_assert(gridtools::is_sid<zavgS_MYY_t>{});
    static_assert(gridtools::is_sid<pp_t>{});
    static_assert(gridtools::is_sid<pnabla_MXX_t>{});
    static_assert(gridtools::is_sid<pnabla_MYY_t>{});
    static_assert(gridtools::is_sid<vol_t>{});
    static_assert(gridtools::is_sid<sign_t>{});

    { // first edge loop (this is the fused version without temporary)
        // ===
        //   for (auto const &t : getEdges(LibTag{}, mesh)) {
        //     double zavg =
        //         (double)0.5 *
        //         (m_sparse_dimension_idx = 0,
        //          reduceVertexToEdge(mesh, t, (double)0.0,
        //                             [&](auto &lhs, auto const &redIdx) {
        //                               lhs += pp(deref(LibTag{}, redIdx), k);
        //                               m_sparse_dimension_idx++;
        //                               return lhs;
        //                             }));
        //     zavgS_MXX(deref(LibTag{}, t), k) =
        //         S_MXX(deref(LibTag{}, t), k) * zavg;
        //     zavgS_MYY(deref(LibTag{}, t), k) =
        //         S_MYY(deref(LibTag{}, t), k) * zavg;
        //   }
        // ===
        auto e2v = gridtools::next::mesh::connectivity<std::tuple<edge, vertex>>(mesh);
        static_assert(gridtools::is_sid<decltype(gridtools::next::connectivity::neighbor_table(e2v))>{});

        auto edge_fields = tu::make<gridtools::sid::composite::
                keys<connectivity_tag, S_MXX_tag, S_MYY_tag, zavgS_MXX_tag, zavgS_MYY_tag>::values>(
            gridtools::next::connectivity::neighbor_table(e2v), S_MXX, S_MYY, zavgS_MXX, zavgS_MYY);
        static_assert(gridtools::sid::concept_impl_::is_sid<decltype(edge_fields)>{});

        auto origin_ptrs = gridtools::sid::get_origin(edge_fields)();
        auto strides = gridtools::sid::get_strides(edge_fields);

        // TODO apply this pattern to the other dimension
        auto edge_iteration_space = gridtools::next::mesh::iteration_space<edge>(
            mesh); // TODO this is broken: I say edge here, but repeat it below
        auto idx_ptr = gridtools::sid::get_origin(edge_iteration_space)();
        for (std::size_t tmpi = gtsh::lower_bound<edge>(edge_iteration_space);
             tmpi < gtsh::upper_bound<edge>(edge_iteration_space);
             ++tmpi,
                         gridtools::sid::shift(
                             idx_ptr, gridtools::at_key<edge>(gridtools::sid::get_strides(edge_iteration_space)), 1)) {
            int i = *idx_ptr;
            auto ptrs = origin_ptrs;

            gridtools::sid::shift(ptrs, gridtools::at_key<edge>(strides), i);
            double acc = 0.;
            { // reduce
                for (int neigh = 0; neigh < static_cast<int>(gridtools::next::connectivity::max_neighbors(e2v));
                     ++neigh) {
                    // body
                    auto absolute_neigh_index = *gridtools::at_key<connectivity_tag>(ptrs);

                    auto pp_ptr = gridtools::sid::get_origin(pp)();
                    gridtools::sid::shift(
                        pp_ptr, gridtools::at_key<vertex>(gridtools::sid::get_strides(pp)), absolute_neigh_index);
                    acc += *pp_ptr;
                    // body end

                    gridtools::sid::shift(ptrs, gridtools::at_key<neighbor>(strides), 1);
                }
                gridtools::sid::shift(
                    ptrs, gridtools::at_key<neighbor>(strides), -gridtools::next::connectivity::max_neighbors(e2v));
            }
            double zavg = 0.5 * acc;
            *gridtools::at_key<zavgS_MXX_tag>(ptrs) = *gridtools::at_key<S_MXX_tag>(ptrs) * zavg;
            *gridtools::at_key<zavgS_MYY_tag>(ptrs) = *gridtools::at_key<S_MYY_tag>(ptrs) * zavg;
        }
    }
    {
        // vertex loop
        // for (auto const &t : getVertices(LibTag{}, mesh)) {
        //     pnabla_MXX(deref(LibTag{}, t), k) =
        //         (m_sparse_dimension_idx = 0,
        //          reduceEdgeToVertex(
        //              mesh, t, (double)0.0, [&](auto &lhs, auto const &redIdx) {
        //                lhs += zavgS_MXX(deref(LibTag{}, redIdx), k) *
        //                       sign(deref(LibTag{}, t), m_sparse_dimension_idx,
        //                       k);
        //                m_sparse_dimension_idx++;
        //                return lhs;
        //              }));
        //   }
        //   for (auto const &t : getVertices(LibTag{}, mesh)) {
        //     pnabla_MYY(deref(LibTag{}, t), k) =
        //         (m_sparse_dimension_idx = 0,
        //          reduceEdgeToVertex(
        //              mesh, t, (double)0.0, [&](auto &lhs, auto const &redIdx) {
        //                lhs += zavgS_MYY(deref(LibTag{}, redIdx), k) *
        //                       sign(deref(LibTag{}, t), m_sparse_dimension_idx,
        //                       k);
        //                m_sparse_dimension_idx++;
        //                return lhs;
        //              }));
        //   }
        auto v2e = gridtools::next::mesh::connectivity<std::tuple<vertex, edge>>(mesh);
        static_assert(gridtools::is_sid<decltype(gridtools::next::connectivity::neighbor_table(v2e))>{});

        auto vertex_fields = tu::make<gridtools::sid::composite::
                keys<connectivity_tag, pnabla_MXX_tag, pnabla_MYY_tag, sign_tag, vol_tag>::values>(
            gridtools::next::connectivity::neighbor_table(v2e), pnabla_MXX, pnabla_MYY, sign, vol);
        static_assert(gridtools::sid::concept_impl_::is_sid<decltype(vertex_fields)>{});

        auto ptrs = gridtools::sid::get_origin(vertex_fields)();
        auto strides = gridtools::sid::get_strides(vertex_fields);

        auto vertex_iteration_space = gridtools::next::mesh::iteration_space<vertex>(mesh);
        for (int i = 0; i < gridtools::next::sid_helper::size<vertex>(vertex_iteration_space); ++i) {
            // for (std::size_t i = 0; i < gridtools::next::connectivity::size(v2e); ++i) {
            *gridtools::at_key<pnabla_MXX_tag>(ptrs) = 0.;
            { // reduce
                for (int neigh = 0; neigh < static_cast<int>(gridtools::next::connectivity::max_neighbors(v2e));
                     ++neigh) {
                    // body
                    auto absolute_neigh_index = *gridtools::at_key<connectivity_tag>(ptrs);
                    if (absolute_neigh_index != gridtools::next::connectivity::skip_value(v2e)) {

                        auto zavgS_MXX_ptr = gridtools::sid::get_origin(zavgS_MXX)();
                        gridtools::sid::shift(zavgS_MXX_ptr,
                            gridtools::at_key<edge>(gridtools::sid::get_strides(zavgS_MXX)),
                            absolute_neigh_index);
                        auto zavgS_MXX_value = *zavgS_MXX_ptr;

                        auto sign_ptr = gridtools::at_key<sign_tag>(ptrs); // if the sparse dimension is tagged with
                                                                           // neighbor, the ptr is already correct
                        auto sign_value = *sign_ptr;

                        *gridtools::at_key<pnabla_MXX_tag>(ptrs) += zavgS_MXX_value * sign_value;
                        // body end
                    }
                    gridtools::sid::shift(ptrs, gridtools::at_key<neighbor>(strides), 1);
                }
                gridtools::sid::shift(ptrs,
                    gridtools::at_key<neighbor>(strides),
                    -gridtools::next::connectivity::max_neighbors(v2e)); // or reset ptr to origin and shift ?
            }
            *gridtools::at_key<pnabla_MYY_tag>(ptrs) = 0.;
            { // reduce
                for (int neigh = 0; neigh < static_cast<int>(gridtools::next::connectivity::max_neighbors(v2e));
                     ++neigh) {
                    // body
                    auto absolute_neigh_index = *gridtools::at_key<connectivity_tag>(ptrs);
                    if (absolute_neigh_index != gridtools::next::connectivity::skip_value(v2e)) {

                        auto zavgS_MYY_ptr = gridtools::sid::get_origin(zavgS_MYY)();
                        gridtools::sid::shift(zavgS_MYY_ptr,
                            gridtools::at_key<edge>(gridtools::sid::get_strides(zavgS_MYY)),
                            absolute_neigh_index);
                        auto zavgS_YY_value = *zavgS_MYY_ptr;

                        // if the sparse dimension is tagged with `neighbor`, the ptr is
                        // already correct
                        auto sign_ptr = gridtools::at_key<sign_tag>(ptrs);
                        auto sign_value = *sign_ptr;

                        *gridtools::at_key<pnabla_MYY_tag>(ptrs) += zavgS_YY_value * sign_value;
                        // body end
                    }
                    gridtools::sid::shift(ptrs, gridtools::at_key<neighbor>(strides), 1);
                }
                // the following or reset ptr to origin and shift ?
                gridtools::sid::shift(
                    ptrs, gridtools::at_key<neighbor>(strides), -gridtools::next::connectivity::max_neighbors(v2e));
            }
            gridtools::sid::shift(ptrs, gridtools::at_key<vertex>(strides), 1);
        }
    }

    // ===
    //   do jedge = 1,dstruct%nb_pole_edges
    //     iedge = dstruct%pole_edges(jedge)
    //     ip2   = dstruct%edges(2,iedge)
    //     ! correct for wrong Y-derivatives in previous loop
    //     pnabla(MYY,ip2) = pnabla(MYY,ip2)+2.0_wp*zavgS(MYY,iedge)
    //   end do
    // ===
    //   {
    //     auto pe2v = gridtools::next::mesh::connectivity<
    //         std::tuple<atlas::pole_edge, vertex>>(mesh);
    //     for (int i = 0; i < gridtools::next::connectivity::size(pe2v);
    //          ++i) {
    //     }
    //   }

    {
        // vertex loop
        // for (auto const &t : getVertices(LibTag{}, mesh)) {
        //     pnabla_MXX(deref(LibTag{}, t), k) =
        //         pnabla_MXX(deref(LibTag{}, t), k) / vol(deref(LibTag{}, t), k);
        //     pnabla_MYY(deref(LibTag{}, t), k) =
        //         pnabla_MYY(deref(LibTag{}, t), k) / vol(deref(LibTag{}, t), k);
        //   }
        auto v2e = gridtools::next::mesh::connectivity<std::tuple<vertex, edge>>(mesh);
        static_assert(gridtools::is_sid<decltype(gridtools::next::connectivity::neighbor_table(v2e))>{});

        auto vertex_fields = tu::make<gridtools::sid::composite::
                keys<connectivity_tag, pnabla_MXX_tag, pnabla_MYY_tag, sign_tag, vol_tag>::values>(
            gridtools::next::connectivity::neighbor_table(v2e), pnabla_MXX, pnabla_MYY, sign, vol);
        static_assert(gridtools::sid::concept_impl_::is_sid<decltype(vertex_fields)>{});

        auto ptrs = gridtools::sid::get_origin(vertex_fields)();
        auto strides = gridtools::sid::get_strides(vertex_fields);

        auto vertex_iteration_space = gridtools::next::mesh::iteration_space<vertex>(mesh);
        for (std::size_t i = 0; i < gridtools::next::sid_helper::size<vertex>(vertex_iteration_space); ++i) {
            // for (std::size_t i = 0; i < gridtools::next::connectivity::size(v2e); ++i) {
            *gridtools::at_key<pnabla_MXX_tag>(ptrs) /= *gridtools::at_key<vol_tag>(ptrs);
            *gridtools::at_key<pnabla_MYY_tag>(ptrs) /= *gridtools::at_key<vol_tag>(ptrs);
            gridtools::sid::shift(ptrs, gridtools::at_key<vertex>(strides), 1);
        }
    }
}
