/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup obj
 */

#include "BKE_blender_version.h"

#include "obj_export_file_writer.hh"
#include "obj_export_mesh.hh"
#include "obj_export_mtl.hh"
#include "obj_export_nurbs.hh"
#include "obj_import_mtl.hh"

namespace blender::io::obj {

/**
 * Write one line of polygon indices as f v1/vt1/vn1 v2/vt2/vn2 ... .
 */
void OBJWriter::write_vert_uv_normal_indices(Span<uint> vert_indices,
                                             Span<uint> uv_indices,
                                             Span<uint> normal_indices,
                                             const uint tot_loop) const
{
  fprintf(outfile_, "f");
  for (uint j = 0; j < tot_loop; j++) {
    fprintf(outfile_,
            " %u/%u/%u",
            vert_indices[j] + index_offset_[VERTEX_OFF],
            uv_indices[j] + index_offset_[UV_VERTEX_OFF],
            normal_indices[j] + index_offset_[NORMAL_OFF]);
  }
  fprintf(outfile_, "\n");
}

/**
 * Write one line of polygon indices as f v1//vn1 v2//vn2 ... .
 */
void OBJWriter::write_vert_normal_indices(Span<uint> vert_indices,
                                          Span<uint> UNUSED(uv_indices),
                                          Span<uint> normal_indices,
                                          const uint tot_loop) const
{
  fprintf(outfile_, "f");
  for (uint j = 0; j < tot_loop; j++) {
    fprintf(outfile_,
            " %u//%u",
            vert_indices[j] + index_offset_[VERTEX_OFF],
            normal_indices[j] + index_offset_[NORMAL_OFF]);
  }
  fprintf(outfile_, "\n");
}

/**
 * Write one line of polygon indices as f v1/vt1 v2/vt2 ... .
 */
void OBJWriter::write_vert_uv_indices(Span<uint> vert_indices,
                                      Span<uint> uv_indices,
                                      Span<uint> UNUSED(normal_indices),
                                      const uint tot_loop) const
{
  fprintf(outfile_, "f");
  for (uint j = 0; j < tot_loop; j++) {
    fprintf(outfile_,
            " %u/%u",
            vert_indices[j] + index_offset_[VERTEX_OFF],
            uv_indices[j] + index_offset_[UV_VERTEX_OFF]);
  }
  fprintf(outfile_, "\n");
}

/**
 *  Write one line of polygon indices as f v1 v2 ... .
 */
void OBJWriter::write_vert_indices(Span<uint> vert_indices,
                                   Span<uint> UNUSED(uv_indices),
                                   Span<uint> UNUSED(normal_indices),
                                   const uint tot_loop) const
{
  fprintf(outfile_, "f");
  for (uint j = 0; j < tot_loop; j++) {
    fprintf(outfile_, " %u", vert_indices[j] + index_offset_[VERTEX_OFF]);
  }
  fprintf(outfile_, "\n");
}

/**
 * Open the OBJ file and write file header.
 */
bool OBJWriter::init_writer(const char *filepath)
{
  outfile_ = fopen(filepath, "w");
  if (!outfile_) {
    return false;
  }
  fprintf(outfile_, "# Blender %s\n# www.blender.org\n", BKE_blender_version_string());
  return true;
}

/**
 * Write file name of Material Library in OBJ file.
 * Also create an empty Material Library file, or truncate the existing one.
 */
void OBJWriter::write_mtllib(const char *obj_filepath) const
{
  char mtl_filepath[FILE_MAX];
  BLI_strncpy(mtl_filepath, obj_filepath, FILE_MAX);
  BLI_path_extension_replace(mtl_filepath, FILE_MAX, ".mtl");

  FILE *mtl_outfile = fopen(mtl_filepath, "w");
  if (!mtl_outfile) {
    fprintf(stderr, "Error opening Material Library file:%s", mtl_filepath);
    return;
  }
  fprintf(stderr, "Material Library: %s\n", mtl_filepath);
  fprintf(mtl_outfile, "# Blender %s\n# www.blender.org\n", BKE_blender_version_string());
  fclose(mtl_outfile);

  /* Split MTL file path into parent directory and filename. */
  char mtl_file_name[FILE_MAXFILE];
  char mtl_dir_name[FILE_MAXDIR];
  BLI_split_dirfile(mtl_filepath, mtl_dir_name, mtl_file_name, FILE_MAXDIR, FILE_MAXFILE);
  fprintf(outfile_, "mtllib %s\n", mtl_file_name);
}

/**
 * Write object name conditionally with mesh and material name.
 */
void OBJWriter::write_object_name(const OBJMesh &obj_mesh_data) const
{
  const char *object_name = obj_mesh_data.get_object_name();

  if (export_params_.export_object_groups) {
    const char *object_mesh_name = obj_mesh_data.get_object_mesh_name();
    if (export_params_.export_materials && export_params_.export_material_groups) {
      const char *object_material_name = obj_mesh_data.get_object_material_name(0);
      fprintf(outfile_, "g %s_%s_%s\n", object_name, object_mesh_name, object_material_name);
    }
    fprintf(outfile_, "g %s_%s\n", object_name, object_mesh_name);
  }
  else {
    fprintf(outfile_, "o %s\n", object_name);
  }
}

/**
 * Write vertex coordinates for all vertices as v x y z .
 */
void OBJWriter::write_vertex_coords(const OBJMesh &obj_mesh_data) const
{
  for (uint i = 0; i < obj_mesh_data.tot_vertices(); i++) {
    float3 vertex = obj_mesh_data.calc_vertex_coords(i, export_params_.scaling_factor);
    fprintf(outfile_, "v %f %f %f\n", vertex[0], vertex[1], vertex[2]);
  }
}

/**
 * Write UV vertex coordinates for all vertices as vt u v .
 * \note UV indices are stored here, but written later.
 */
void OBJWriter::write_uv_coords(OBJMesh &obj_mesh_data) const
{
  Vector<std::array<float, 2>> uv_coords;
  obj_mesh_data.store_uv_coords_and_indices(uv_coords);

  for (const std::array<float, 2> &uv_vertex : uv_coords) {
    fprintf(outfile_, "vt %f %f\n", uv_vertex[0], uv_vertex[1]);
  }
}

/**
 * Write vertex normals for smooth-shaded polygons, and face normals otherwise, as vn x y z .
 */
void OBJWriter::write_poly_normals(OBJMesh &obj_mesh_data) const
{
  obj_mesh_data.ensure_mesh_normals();
  for (uint i = 0; i < obj_mesh_data.tot_polygons(); i++) {
    if (obj_mesh_data.is_ith_poly_smooth(i)) {
      for (int j = 0; j < obj_mesh_data.ith_poly_totloop(i); j++) {
        float3 vertex_normal = obj_mesh_data.calc_vertex_normal(j);
        fprintf(outfile_, "vn %f %f %f\n", vertex_normal[0], vertex_normal[1], vertex_normal[2]);
      }
    }
    else {
      float3 poly_normal = obj_mesh_data.calc_poly_normal(i);
      fprintf(outfile_, "vn %f %f %f\n", poly_normal[0], poly_normal[1], poly_normal[2]);
    }
  }
}

/**
 * Write smooth group if the polygon at given index is shaded smooth and export settings specify
 * so. If the polygon is not shaded smooth, write "off".
 */
void OBJWriter::write_smooth_group(const OBJMesh &obj_mesh_data,
                                   const uint poly_index,
                                   int &r_last_face_smooth_group) const
{
  int curr_group = 0;
  if (!export_params_.export_smooth_groups && obj_mesh_data.is_ith_poly_smooth(poly_index)) {
    /* Smooth group calculation is disabled, but face is smooth. */
    curr_group = 1;
  }
  else if (obj_mesh_data.is_ith_poly_smooth(poly_index)) {
    /* Smooth group calc enabled and face is smooth and so find the group. */
    curr_group = obj_mesh_data.ith_smooth_group(poly_index);
  }
  if (curr_group == r_last_face_smooth_group) {
    /* Group has already been written including s off. */
    return;
  }
  if (curr_group == 0) {
    fprintf(outfile_, "s off\n");
    r_last_face_smooth_group = curr_group;
    return;
  }
  fprintf(outfile_, "s %d\n", curr_group);
  r_last_face_smooth_group = curr_group;
}

/**
 * Write material name and material group of a face in the OBJ file.
 * \note It doesn't write to the material library.
 */
void OBJWriter::write_poly_material(const OBJMesh &obj_mesh_data,
                                    const uint poly_index,
                                    short &r_last_face_mat_nr) const
{
  if (!export_params_.export_materials || obj_mesh_data.tot_col() <= 0) {
    return;
  }
  const short mat_nr = obj_mesh_data.ith_poly_matnr(poly_index);
  /* Whenever a face with a new material is encountered, write its material and/or group, otherwise
   * pass. */
  if (r_last_face_mat_nr != mat_nr) {
    const char *mat_name = obj_mesh_data.get_object_material_name(mat_nr + 1);
    if (export_params_.export_material_groups) {
      const char *object_name = obj_mesh_data.get_object_name();
      const char *object_mesh_name = obj_mesh_data.get_object_mesh_name();
      fprintf(outfile_, "g %s_%s_%s\n", object_name, object_mesh_name, mat_name);
    }
    fprintf(outfile_, "usemtl %s\n", mat_name);
    r_last_face_mat_nr = mat_nr;
  }
}

/**
 * Write the name of the deform group of a face. If no vertex group is found in the face, "off" is
 * written.
 */
void OBJWriter::write_vertex_group(const OBJMesh &obj_mesh_data,
                                   const uint poly_index,
                                   short &last_face_vertex_group) const
{
  if (!export_params_.export_vertex_groups) {
    return;
  }

  const char *def_group_name = obj_mesh_data.get_poly_deform_group_name(poly_index,
                                                                        last_face_vertex_group);
  if (!def_group_name) {
    /* Don't write the name of the group again. If set once, the group name changes only when a new
     * one is encountered. */
    return;
  }
  fprintf(outfile_, "g %s\n", def_group_name);
}

/**
 * Define and write face elements with at least vertex indices, and conditionally with UV vertex
 * indices and face normal indices. Also write groups: smooth, vertex, material.
 *  \note UV indices are stored while writing UV vertices.
 */
void OBJWriter::write_poly_elements(const OBJMesh &obj_mesh_data)
{
  /* -1 has no significant value, it can be any negative number. */
  int last_face_smooth_group = -1;
  /* -1 is used to denote face having no vertex group. It can be any _other_ negative
   * number. */
  short last_face_vertex_group = -2;
  /* -1 has no significant value, it can be any negative number. */
  short last_face_mat_nr = -1;

  void (OBJWriter::*func_vert_uv_normal_indices)(Span<uint> vert_indices,
                                                 Span<uint> uv_indices,
                                                 Span<uint> normal_indices,
                                                 const uint tot_loop) const = nullptr;
  if (export_params_.export_normals) {
    if (export_params_.export_uv && (obj_mesh_data.tot_uv_vertices() > 0)) {
      /* Write both normals and UV indices. */
      func_vert_uv_normal_indices = &OBJWriter::write_vert_uv_normal_indices;
    }
    else {
      /* Write normals indices. */
      func_vert_uv_normal_indices = &OBJWriter::write_vert_normal_indices;
    }
  }
  else {
    /* Write UV indices. */
    if (export_params_.export_uv && (obj_mesh_data.tot_uv_vertices() > 0)) {
      func_vert_uv_normal_indices = &OBJWriter::write_vert_uv_indices;
    }
    else {
      /* Write neither normals nor UV indices. */
      func_vert_uv_normal_indices = &OBJWriter::write_vert_indices;
    }
  }

  Vector<uint> vertex_indices;
  Vector<uint> normal_indices;
  /* Reset for every object. */
  tot_normals_ = 0;
  for (uint i = 0; i < obj_mesh_data.tot_polygons(); i++) {
    const int totloop = obj_mesh_data.ith_poly_totloop(i);
    normal_indices.resize(totloop);
    vertex_indices.resize(totloop);
    obj_mesh_data.calc_poly_vertex_indices(i, vertex_indices);

    if (obj_mesh_data.is_ith_poly_smooth(i)) {
      for (int j = 0; j < totloop; j++) {
        /* This is guaranteed because normals are written
         * by going over `MLoop`s in the same order. */
        normal_indices[j] = tot_normals_ + j + 1;
      }
      tot_normals_ += totloop;
    }
    else {
      for (int j = 0; j < totloop; j++) {
        normal_indices[j] = tot_normals_ + 1;
      }
      tot_normals_ += 1;
    }

    write_smooth_group(obj_mesh_data, i, last_face_smooth_group);
    write_vertex_group(obj_mesh_data, i, last_face_vertex_group);
    write_poly_material(obj_mesh_data, i, last_face_mat_nr);
    (this->*func_vert_uv_normal_indices)(vertex_indices, obj_mesh_data.uv_indices(i), normal_indices, totloop);
  }
}

/**
 * Write loose edges of a mesh, a curve converted to mesh, or a primitive circle as l v1 v2 .
 */
void OBJWriter::write_loose_edges(const OBJMesh &obj_mesh_data) const
{
  obj_mesh_data.ensure_mesh_edges();
  for (uint edge_index = 0; edge_index < obj_mesh_data.tot_edges(); edge_index++) {
    std::optional<std::array<int, 2>> vertex_indices = obj_mesh_data.calc_edge_vert_indices(
        edge_index);
    if (vertex_indices) {
      fprintf(outfile_,
              "l %u %u\n",
              (*vertex_indices)[0] + index_offset_[VERTEX_OFF],
              (*vertex_indices)[1] + index_offset_[VERTEX_OFF]);
    }
  }
}

/**
 * Write a NURBS curve to the OBJ file in parameter form.
 */
void OBJWriter::write_nurbs_curve(const OBJCurve &obj_nurbs_data) const
{
  const int tot_nurbs = obj_nurbs_data.tot_nurbs();
  for (int i = 0; i < tot_nurbs; i++) {
    /* Total control points in a nurbs. */
    int tot_points = obj_nurbs_data.get_nurbs_points(i);
    for (int point_idx = 0; point_idx < tot_points; point_idx++) {
      float3 point_coord = obj_nurbs_data.calc_nurbs_point_coords(
          i, point_idx, export_params_.scaling_factor);
      fprintf(outfile_, "v %f %f %f\n", point_coord[0], point_coord[1], point_coord[2]);
    }

    const char *nurbs_name = obj_nurbs_data.get_curve_name();
    int nurbs_degree = obj_nurbs_data.get_nurbs_degree(i);
    /* Number of vertices in the curve + degree of the curve if it is cyclic. */
    int curv_num = obj_nurbs_data.get_nurbs_num(i);

    fprintf(outfile_, "g %s\ncstype bspline\ndeg %d\n", nurbs_name, nurbs_degree);
    /**
     * curv_num indices into the point vertices above, in relative indices.
     * 0.0 1.0 -1 -2 -3 -4 for a non-cyclic curve with 4 points.
     * 0.0 1.0 -1 -2 -3 -4 -1 -2 -3 for a cyclic curve with 4 points.
     */
    fprintf(outfile_, "curv 0.0 1.0");
    for (int i = 0; i < curv_num; i++) {
      fprintf(outfile_, " %d", -1 * ((i % tot_points) + 1));
    }
    fprintf(outfile_, "\n");

    /**
     * In parm u line: between 0 and 1, curv_num + 2 equidistant numbers are inserted.
     */
    fprintf(outfile_, "parm u 0.000000 ");
    for (int i = 1; i <= curv_num + 2; i++) {
      fprintf(outfile_, "%f ", 1.0f * i / (curv_num + 2 + 1));
    }
    fprintf(outfile_, "1.000000\n");

    fprintf(outfile_, "end\n");
  }
}

/**
 *  When there are multiple objects in a frame, the indices of previous objects' coordinates or
 * normals add up.
 */
void OBJWriter::update_index_offsets(const OBJMesh &obj_mesh_data)
{
  index_offset_[VERTEX_OFF] += obj_mesh_data.tot_vertices();
  index_offset_[UV_VERTEX_OFF] += obj_mesh_data.tot_uv_vertices();
  index_offset_[NORMAL_OFF] = tot_normals_;
  tot_normals_ = 0;
}

/**
 * Open the MTL file in append mode.
 */
MTLWriter::MTLWriter(const char *obj_filepath)
{
  char mtl_filepath[FILE_MAX];
  BLI_strncpy(mtl_filepath, obj_filepath, FILE_MAX);
  BLI_path_extension_replace(mtl_filepath, FILE_MAX, ".mtl");
  mtl_outfile_ = fopen(mtl_filepath, "a");
  if (!mtl_outfile_) {
    fprintf(stderr, "Error in opening file at %s\n", mtl_filepath);
    return;
  }
}

MTLWriter::~MTLWriter()
{
  fclose(mtl_outfile_);
}

void MTLWriter::append_materials(const OBJMesh &mesh_to_export)
{
  if (!mtl_outfile_) {
    /* Error logging in constructor. */
    return;
  }
  Vector<MTLMaterial> mtl_materials;
  MaterialWrap mat_wrap(mesh_to_export, mtl_materials);
  mat_wrap.fill_materials();

  for (const MTLMaterial &mtl_material : mtl_materials) {
    fprintf(mtl_outfile_, "\nnewmtl %s\n", mtl_material.name.c_str());
    fprintf(mtl_outfile_, "Ns %.6f\n", mtl_material.Ns);
    fprintf(mtl_outfile_,
            "Ka %0.6f %0.6f %0.6f\n",
            mtl_material.Ka[0],
            mtl_material.Ka[1],
            mtl_material.Ka[2]);
    fprintf(mtl_outfile_,
            "Kd %0.6f %0.6f %0.6f\n",
            mtl_material.Kd[0],
            mtl_material.Kd[1],
            mtl_material.Kd[2]);
    fprintf(mtl_outfile_,
            "Ks %0.6f %0.6f %0.6f\n",
            mtl_material.Ks[0],
            mtl_material.Ks[0],
            mtl_material.Ks[0]);
    fprintf(mtl_outfile_,
            "Ke %0.6f %0.6f %0.6f\n",
            mtl_material.Ke[0],
            mtl_material.Ke[1],
            mtl_material.Ke[2]);
    fprintf(mtl_outfile_,
            "Ni %0.6f\nd %.6f\nillum %d\n",
            mtl_material.Ni,
            mtl_material.d,
            mtl_material.illum);
    for (const Map<const std::string, tex_map_XX>::Item &texture_map :
         mtl_material.texture_maps.items()) {
      if (texture_map.value.image_path.empty()) {
        continue;
      }
      std::string map_bump_strength;
      if (texture_map.key == "map_Bump" && mtl_material.map_Bump_strength > 0.0001f) {
        map_bump_strength = " -bm " + std::to_string(mtl_material.map_Bump_strength);
      }
      /* Always keep only one space between options since filepaths may have leading spaces too.
       * map_Bump string has its leading space. */
      fprintf(mtl_outfile_,
              "%s -o %.6f %.6f %.6f -s %.6f %.6f %.6f%s %s\n",
              texture_map.key.c_str(),
              texture_map.value.translation[0],
              texture_map.value.translation[1],
              texture_map.value.translation[2],
              texture_map.value.scale[0],
              texture_map.value.scale[1],
              texture_map.value.scale[2],
              map_bump_strength.c_str(), /* Can be empty. */
              texture_map.value.image_path.c_str());
    }
  }
}
}  // namespace blender::io::obj
