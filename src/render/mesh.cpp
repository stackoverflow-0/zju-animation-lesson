#include "mesh.hpp"
#include <format>
#include <queue>

namespace assimp_model
{
    constexpr int animation_texture_width = 1024;

    auto Mesh::append_mesh(std::vector<Vertex> &append_vertices, std::vector<unsigned int> &append_indices, std::vector<glm::vec2> &append_driven_bone_offset, std::vector<std::vector<driven_bone>> &append_driven_bone_and_weight) -> void
    {
        auto indices_offset = vertices.size();
        float driven_bone_offset_offset{0.0};
        if (!vertices.empty())
            driven_bone_offset_offset = vertices.back().bone_weight_offset.x + vertices.back().bone_weight_offset.y;
        int i{0};
        for (auto &v : append_vertices)
        {

            append_driven_bone_offset[i].x += driven_bone_offset_offset;
            v.bone_weight_offset = append_driven_bone_offset[i];
            i++;
            vertices.emplace_back(v);
        
        }

        for (auto &b_and_w : append_driven_bone_and_weight)
        {
            for (auto &bw : b_and_w)
            {
                bone_id_and_weight.emplace_back(float(bw.driven_bone_id), bw.driven_bone_weight);
            }
        }
        for (auto i : append_indices)
        {
            indices.emplace_back(indices_offset + i);
        }
    }

    auto Mesh::setup_mesh() -> void
    {
        // create buffers/arrays
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        // load data into vertex buffers
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        // A great thing about structs is that their memory layout is sequential for all its items.
        // The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
        // again translates to 3/2 floats which translates to a byte array.
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // set the vertex attribute pointers
        // vertex Positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
        // vertex normals
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));
        // vertex texture coords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, texcoords));
        // vertex animation texture coords
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, bone_weight_offset));
        glBindVertexArray(0);

        // glEnableVertexAttribArray(4);
        // glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, driven_bone_weight));
        // GLuint blockIndex = glGetUniformBlockIndex(programHandle, "BlobSettings");
        glGenTextures(1, &bone_weight_texture);

        glBindTexture(GL_TEXTURE_2D, bone_weight_texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, animation_texture_width, bone_id_and_weight.size() / 2 / animation_texture_width + 1, 0, GL_RGBA, GL_FLOAT, bone_id_and_weight.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);

        // glActiveTexture(GL_TEXTURE0);
        // glBindTexture(GL_TEXTURE_2D, bone_weight_texture);
    }

    auto Model::load_model(std::string const path) -> bool
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(ROOT_DIR + path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // check for errors
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return false;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('/'));

        auto processSkeleton = [&]() -> void
        {

            auto bone_root = scene->mRootNode;

            auto walk_bone_tree = [&]() -> void
            {
                std::queue<aiNode *> bone_to_be_walk({bone_root});

                while (!bone_to_be_walk.empty())
                {
                    auto bone_node = bone_to_be_walk.front();
                    bone_to_be_walk.pop();

                    std::string bone_name = bone_node->mName.C_Str();
                    if (bone_name_to_id.find(bone_name) == bone_name_to_id.end())
                        bone_name_to_id.emplace(bone_name, bone_name_to_id.size());

                    // std::cout << std::format("bone name : {:s}\n", bone_name);
                    std::string parent_name{};
                    int parent_id{-1};
                    std::vector<int> child_id{};

                    if (bone_node->mParent != nullptr)
                    {
                        parent_name = bone_node->mParent->mName.C_Str();
                        if (bone_name_to_id.find(parent_name) != bone_name_to_id.end())
                        {
                            parent_id = bone_name_to_id.at(parent_name);
                        }
                    }

                    for (auto bone_child_i = 0; bone_child_i < bone_node->mNumChildren; bone_child_i++)
                    {
                        bone_to_be_walk.push(bone_node->mChildren[bone_child_i]);
                        // bone_name_to_id.at(bone_node->mChildren[bone_child_i]->mName.C_Str());
                        std::string child_name = bone_node->mChildren[bone_child_i]->mName.C_Str();

                        bone_name_to_id.emplace(child_name, bone_name_to_id.size());

                        if (bone_name_to_id.find(child_name) != bone_name_to_id.end())
                            child_id.emplace_back(bone_name_to_id.at(child_name));
                    }
                    bones.resize(bone_name_to_id.size());
                    bones[bone_name_to_id.at(bone_name)] = {{}, parent_id, bone_name, child_id};
                }
            };

            walk_bone_tree();

            if (scene->HasAnimations())
            {
                auto anim_num = scene->mNumAnimations;
                
                tracks.resize(anim_num);

                std::cout << std::format("anim count {:d}\n", anim_num);

                for (auto anim_id = 0; anim_id < anim_num; anim_id++)
                {

                    auto &track = tracks[anim_id];

                    auto anim = scene->mAnimations[anim_id];
                    auto anim_channel_num = anim->mNumChannels;
                    // assert(anim != nullptr);
                    // aiAnimation anim_ins = *scene->mAnimations[anim_id];
                    std::cout << std::format("anim track name {:s}\n", anim->mName.C_Str());

                    track.track_name = std::string(anim->mName.C_Str());
                    track.duration = anim->mDuration;
                    track.frame_per_second = anim->mTicksPerSecond;
                    track.channels.resize(bone_name_to_id.size());
                    std::cout << std::format("anim frames {:d}\n", anim->mChannels[0]->mNumRotationKeys);
                    for (auto i = 0; i < anim_channel_num; i++)
                    {
                        auto& channel_node = anim->mChannels[i];
                        assert(bone_name_to_id.find(channel_node->mNodeName.C_Str()) != bone_name_to_id.end());
                        auto& channel_id = bone_name_to_id.at(channel_node->mNodeName.C_Str());
                        auto& channel = track.channels[channel_id];

                        

                        channel.trans_matrix.resize(channel_node->mNumRotationKeys, glm::identity<glm::mat4x4>());
                        channel.times.resize(channel_node->mNumRotationKeys);

                        for (auto key_id = 0; key_id < channel_node->mNumRotationKeys; key_id++)
                        {
                            auto& rot = channel_node->mRotationKeys[key_id].mValue;
                            auto& trans = channel_node->mPositionKeys[key_id].mValue;
                            auto& scale = channel_node->mScalingKeys[key_id].mValue;
                            // auto rot_mat = aiMatrix4x4(rot.GetMatrix());
                            
                            channel.trans_matrix[key_id] = 
                                glm::translate(glm::identity<glm::mat4x4>(), glm::vec3(trans.x, trans.y, trans.z)) 
                                * glm::toMat4(glm::quat(rot.w, rot.x, rot.y, rot.z)) 
                                * glm::scale(glm::identity<glm::mat4x4>(), glm::vec3(scale.x, scale.y, scale.z));

                            channel.times[key_id] = channel_node->mRotationKeys[key_id].mTime;
                        }
                    }
                }
            }
        };

        processSkeleton();

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene);
        // uniform_mesh.setup_mesh();
        setup_model();
        return true;
    }

    auto Model::processNode(aiNode *node, const aiScene *scene) -> void
    {
        // process each mesh located at the current node
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene.
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            processMesh(mesh, scene);
            // break;
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
            // break;
        }
    }

    auto Model::processMesh(aiMesh *mesh, const aiScene *scene) -> void
    {
        // data to fill
        static int vertex_idx = 0;
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        // Walk through each of the mesh's vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
                              // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.position = vector;
            // normals
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.normal = vector;
            // texture coordinates
            if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.texcoords = vec;
            }
            else
                vertex.texcoords = glm::vec2(0.0f, 0.0f);

            vertex_idx++;

            vertices.push_back(vertex);
        }
        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        std::vector<std::vector<driven_bone>> driven_bone_and_weight{};
        std::vector<glm::vec2> driven_bone_offset{};
        driven_bone_and_weight.resize(vertices.size());
        driven_bone_offset.resize(vertices.size());

        auto &mesh_bones = mesh->mBones;
        auto bone_num = mesh->mNumBones;

        for (auto i = 0; i < bone_num; i++)
        {
            auto &bone = mesh_bones[i];
            auto bone_id = bone_name_to_id.at(bone->mName.C_Str());
            auto bone_drive_vert_num = bone->mNumWeights;

            auto &bone_bind_pose = bone->mOffsetMatrix;
            auto bone_bind_pose_mat = glm::mat4x4{
                bone_bind_pose.a1, bone_bind_pose.a2, bone_bind_pose.a3, bone_bind_pose.a4,
                bone_bind_pose.b1, bone_bind_pose.b2, bone_bind_pose.b3, bone_bind_pose.b4,
                bone_bind_pose.c1, bone_bind_pose.c2, bone_bind_pose.c3, bone_bind_pose.c4,
                bone_bind_pose.d1, bone_bind_pose.d2, bone_bind_pose.d3, bone_bind_pose.d4,
            };
            if (bones[bone_id].bind_pose_offset_mat == glm::mat4x4{}) {
                bones[bone_id].bind_pose_offset_mat = bone_bind_pose_mat;
            }
            else if (bones[bone_id].bind_pose_offset_mat != bone_bind_pose_mat)
            {
                // bones[bone_id].bind_pose_local = bone_bind_pose_mat;
                std::cout << "error: bind pose conflict\n";
            }

            for (auto j = 0; j < bone_drive_vert_num; j++)
            {
               auto vert_id = bone->mWeights[j].mVertexId;
                auto vert_weight = bone->mWeights[j].mWeight;
                auto &b_and_w = driven_bone_and_weight[vert_id];

                b_and_w.emplace_back(driven_bone{bone_id, vert_weight});
            }
        }


        auto i{0};
        auto base_offset{0};
        for (auto &b_and_w : driven_bone_and_weight)
        {
            auto weight_sum{0.0f};
            for (auto &bw : b_and_w)
            {
                weight_sum += bw.driven_bone_weight;
            }
            if (std::fabs(weight_sum - 1.0f) > 0.01f) {
                std::cout << "vert weight < 1.0f\n";
            }
            driven_bone_offset[i] = glm::vec2{base_offset, b_and_w.size()};
            base_offset += b_and_w.size();
            i++;
        }
        uniform_mesh.append_mesh(vertices, indices, driven_bone_offset, driven_bone_and_weight);
    }

    auto Model::create_bind_pose_matrix_texure() -> void
    {
        std::vector<glm::mat4x4> tmp_bind_mat_array{};
        // tmp_bind_mat_array.resize(bones.size());
        for (auto &bone : bones)
        {
            // std::cout << bone.bind_pose_world[0][0] << std::endl;
            tmp_bind_mat_array.emplace_back(bone.bind_pose_offset_mat);

        }

        glGenTextures(1, &bind_pose_texture);
        glBindTexture(GL_TEXTURE_2D, bind_pose_texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, animation_texture_width, tmp_bind_mat_array.size() * 4 / animation_texture_width + 1, 0, GL_RGBA, GL_FLOAT, tmp_bind_mat_array.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    auto Model::create_track_anim_matrix_texure(int track_index) -> void
    {
        assert(track_index < tracks.size());

        auto &channels = tracks[track_index].channels;
        auto &track_anim_texture = tracks[track_index].track_anim_texture;

        auto channel_frame_num{0};
        for (auto& c: channels) {
            if (c.trans_matrix.size() > channel_frame_num)
                channel_frame_num = c.trans_matrix.size();
        }
        auto channel_num = channels.size();

        std::vector<glm::mat4x4> tmp_anim_pose_frames{};

        tmp_anim_pose_frames.resize(channel_num * channel_frame_num, glm::identity<glm::mat4x4>());

        auto get_world_transform = [&](int bone_id, int frame_id) -> glm::mat4x4 {
            auto bone_it{bone_id};
            glm::mat4x4 world_transform = glm::identity<glm::mat4x4>();
            while (bone_it != -1) {

                auto frame_sz = channels[bone_it].trans_matrix.size();

                if (frame_sz == 0) {
                    bone_it = bones[bone_it].parent_id;
                    continue;
                }

                auto r_frame_id = frame_id >= frame_sz ? frame_sz - 1 : frame_id;

                if (channels[bone_it].trans_matrix[r_frame_id] != glm::mat4x4(0)) {
                    world_transform =  channels[bone_it].trans_matrix[r_frame_id] * world_transform;
                }

                bone_it = bones[bone_it].parent_id;
                // break;
            }
            return world_transform;
        };

        for (auto i = 0; i < channel_frame_num; i++)
        {
            for (auto cid = 0; cid < channel_num; cid++)
            {
                tmp_anim_pose_frames[i * channel_num + cid] = get_world_transform(cid, i);
            }
        }
        glGenTextures(1, &track_anim_texture);
        glBindTexture(GL_TEXTURE_2D, track_anim_texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, channel_num * 4, channel_frame_num, 0, GL_RGBA, GL_FLOAT, tmp_anim_pose_frames.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);

    }

    auto Model::bind_textures() -> void
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, uniform_mesh.bone_weight_texture);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bind_pose_texture);
        auto track_index{0};
        for (auto &track : tracks)
        {
            glActiveTexture(GL_TEXTURE2 + track_index);
            glBindTexture(GL_TEXTURE_2D, track.track_anim_texture);
            track_index++;
        }
    }
} // namespace model
