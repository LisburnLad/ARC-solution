#include "precompiled_stl.hpp"
#include <chrono>
using namespace std;
#include "utils.hpp"
#include "read.hpp"
#include "normalize.hpp"
#include "core_functions.hpp"
#include "image_functions.hpp"
#include "image_functions2.hpp"

#include "visu.hpp"

#include "brute2.hpp"
#include "pieces.hpp"
#include "timer.hpp"

extern int print_nodes;

ull hashVec(const vector<int> &vec)
{
  ull r = 1;
  for (int v : vec)
  {
    r = r * 1069388789821391921 + v;
  }
  return r;
}

/**
 * Constructs a set of pieces based on the given input.
 *
 * @param dag The vector of DAGs representing the problem.
 * @param train The vector of pairs of images used for training.
 * @param out_sizes The vector of output sizes.
 * @return The constructed Pieces object.
 */
Pieces makePieces2(vector<DAG> &dag, vector<pair<Image, Image>> train, vector<point> out_sizes)
{
  Timer set_time, piece_time, child_time;

  Pieces pieces;

  vector<int> &mem = pieces.mem;
  vector<int> depth_mem;

  // Get the number of DAGs (Directed Acyclic Graphs)
  int dags = dag.size();

  // Initialize a hash map to keep track of seen vectors
  TinyHashMap seen;
  // Initialize a vector of queues to manage nodes at different depths
  vector<queue<int>> q;

  // Lambda function to add a vector to the memory and update depth information
  auto add = [&](int d, const vector<int> &v)
  {
    // Ensure the vector size matches the number of DAGs
    assert(v.size() == dags);

    // Start timing the insertion process
    set_time.start();
    // Insert the hashed vector into the seen hash map
    auto [memi, inserted] = seen.insert(hashVec(v), (int)mem.size());
    // Stop timing the insertion process
    set_time.stop();

    // If the vector was inserted for the first time
    if (inserted)
    {
      // Add each element of the vector to the memory
      for (int i : v)
      {
        mem.push_back(i);
      }
      // Record the depth of the memory
      depth_mem.push_back(d);
    }

    // If the vector was inserted or the current depth is less than the recorded depth
    if (inserted || depth_mem[memi / dags] > d)
    {
      // Update the depth of the memory
      depth_mem[memi / dags] = d;
      // Ensure the queue has enough space for the current depth
      while (q.size() <= d)
        q.push_back({});
      // Add the memory index to the queue at the current depth
      q[d].push(memi);
    }
  };

  // Iterate over the given nodes in the first DAG
  for (int i = 0; i < dag[0].givens; i++)
  {
    // Create a vector with the current index repeated for each DAG
    vector<int> v(dags, i);
    // Add the vector to the memory with the depth of the current tiny node
    add(dag[0].tiny_node[i].depth, v);
  }

  // Initialize a vector to store slow child pairs for each training example
  vector<vector<pair<int, int>>> slow_child(train.size() + 1);

  // List to store new indices and their associated vectors
  vector<pair<int, vector<int>>> newi_list;

  // Start timing the piece processing
  piece_time.start();

  // Iterate over each depth level in the queue
  for (int depth = 0; depth < q.size(); depth++)
  {
    // Process all elements at the current depth
    while (q[depth].size())
    {
      // Get the memory index from the front of the queue
      int memi = q[depth].front();
      q[depth].pop();

      // Skip if the current depth is greater than the recorded depth
      if (depth > depth_mem[memi / dags])
        continue;

      // Ensure the current depth matches the recorded depth
      assert(depth == depth_mem[memi / dags]);

      // Extract the indices from the memory
      vector<int> ind(mem.begin() + memi, mem.begin() + memi + dags);

      {
        int ok = 1, maxdepth = -1;
        // Check if all nodes are pieces and find the maximum depth
        for (int i = 0; i < dags; i++)
        {
          maxdepth = max(maxdepth, (int)dag[i].tiny_node[ind[i]].depth);
          ok &= dag[i].tiny_node[ind[i]].ispiece;
        }
        // If all nodes are pieces and the maximum depth is greater than or equal to the current depth
        if (ok && maxdepth >= depth)
        {
          // Create a new piece and add it to the pieces list
          Piece3 p;
          p.memi = memi;
          p.depth = depth;
          pieces.piece.push_back(p);
        }
        // Skip further processing if the maximum depth is less than the current depth
        if (maxdepth < depth)
          continue;
      }

      // Clear the list of new indices
      newi_list.clear();

      // Start timing the child processing
      child_time.start();
      {
        // Populate the slow_child vector for each training example
        for (int i = 0; i <= train.size(); i++)
          dag[i].tiny_node[ind[i]].child.legacy(slow_child[i]);

        // Initialize vectors for new indices and current indices into child[]
        vector<int> newi(dags), ci(dags);
        int fi = 0;

        // Iterate to find new indices
        while (1)
        {
        next_iteration:
          for (int i = 0; i <= train.size(); i++)
          {
            auto &child = slow_child[i];
            // Skip children that have already been processed
            while (ci[i] < child.size() && child[ci[i]].first < fi)
              ci[i]++;
            // If all children have been processed, finish the iteration
            if (ci[i] == child.size())
              goto finish;

            // Get the next available child index
            int next_available_fi = child[ci[i]].first;
            if (next_available_fi > fi)
            {
              fi = next_available_fi;
              goto next_iteration;
            }
            else
            {
              // Update the new index for the current DAG
              newi[i] = child[ci[i]].second;

              // If the new index is invalid, move to the next iteration
              if (newi[i] == -1)
              {
                fi++;
                goto next_iteration;
              }
            }
          }
          // Add the new index and its associated vector to the list
          newi_list.emplace_back(fi, newi);
          fi++;
        }
      finish:;
      }
      // Stop timing the child processing
      child_time.stop();

      // Iterate over each new index and its associated vector in the newi_list
      for (auto &[fi, newi] : newi_list)
      {
        // This block is currently disabled (if (0))
        // if (0)
        // {
        //   // Get the index of the training set
        //   int i = train.size();

        //   // Get the child index for the current node and function index
        //   int to = dag[i].tiny_node.getChild(ind[i], fi);

        //   // If the child index is None
        //   if (to == TinyChildren::None)
        //   {
        //     // Get the name of the function
        //     string name = dag[i].funcs.getName(fi);
        //     // If the function name starts with "Move"
        //     if (name.substr(0, 4) == "Move")
        //     {
        //       // Apply the function to get the new index
        //       newi[i] = dag[i].applyFunc(ind[i], fi);
        //       // If the new index is valid and output sizes are defined
        //       if (newi[i] != -1 && out_sizes.size())
        //         // Apply the "embed 1" function to the new index
        //         dag[i].applyFunc(newi[i], dag[i].funcs.findfi("embed 1"));
        //     }
        //     else
        //       // Skip to the next iteration if the function name does not start with "Move"
        //       continue;
        //   }
        //   else
        //   {
        //     // Set the new index to the child index
        //     newi[i] = to;
        //   }
        //   // Skip to the next iteration if the new index is invalid
        //   if (newi[i] == -1)
        //     continue;
        // }

        // Initialize the new depth to -1
        int new_depth = -1;
        // Calculate the maximum depth among all DAGs for the new indices
        for (int i = 0; i < dags; i++)
        {
          new_depth = max(new_depth, (int)dag[i].tiny_node[newi[i]].depth);
        }

        // Get the cost of the current function
        int cost = dag[0].funcs.cost[fi];

        // If the new depth is greater than or equal to the current depth plus the cost
        if (new_depth >= depth + cost)
        {
          // Add the new indices to the memory with the updated depth
          add(depth + cost, newi);
        }
      }
    }
  }

  // Stop timing the piece processing
  piece_time.stop();
  // Print the timing information for the piece loop
  piece_time.print("Piece loop time");
  // Print the timing information for the set operations
  set_time.print("Set time");
  // Print the timing information for the child processing
  child_time.print("Child looking time");

  // // Lambda function to look for specific nodes in the DAGs based on a list of function names
  // auto lookFor = [&](vector<string> name_list)
  // {
  //   // Vector to store the images of the nodes found
  //   vector<Image> look_imgs;
  //   // Vector to store the indices of the nodes found
  //   vector<int> look_v;

  //   // Initialize the DAG index
  //   int di = 0;

  //   // Iterate over each DAG
  //   for (DAG &d : dag)
  //   {
  //     // Start from the root node
  //     int p = 0;
  //     // Iterate over each function name in the name list
  //     for (string name : name_list)
  //     {
  //       // Find the function index for the current name
  //       int fi = d.funcs.findfi(name);

  //       // Get the child index for the current node and function index
  //       int ret = d.tiny_node.getChild(p, fi);
  //       // Ensure the child index is valid
  //       assert(ret >= 0);
  //       // Debugging output (currently disabled)
  //       if (0)
  //       {
  //         DEBUG_TRACE( p << ' ' << di << " / " << train.size() << endl);
  //       }
  //       // Update the current node to the child node
  //       p = ret;
  //       // Ensure the current node index is valid
  //       assert(p != -1);
  //     }
  //     // Add the final node index to the look_v vector
  //     look_v.push_back(p);
  //     // Add the image of the final node to the look_imgs vector
  //     look_imgs.push_back(d.getImg(p));
  //     // Increment the DAG index
  //     di++;
  //   }
  //   // Check if the vector of node indices has been seen before
  //   if (!seen.insert(hashVec(look_v), 0).second)
  //     // Output a message if the indices have been found before
  //     DEBUG_TRACE( "Found indices" << endl);
  // };

  if (out_sizes.size() && print_nodes)
  {
    int nodes = 0;
    for (DAG &d : dag)
      nodes += d.tiny_node.size();
    DEBUG_TRACE( "Nodes:  " << nodes << endl);
    DEBUG_TRACE( "Pieces: " << pieces.piece.size() << endl);
  }
  pieces.dag = move(dag);

  for (Piece3 &p : pieces.piece)
  {
    for (int i = 0; i < pieces.dag.size(); i++)
    {
      int *ind = &mem[p.memi];
      assert(ind[i] >= 0 && ind[i] < pieces.dag[i].tiny_node.size());
    }
  }

  return pieces;
}
