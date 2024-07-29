#include "precompiled_stl.hpp"
using namespace std;
#include "utils.hpp"
#include "core_functions.hpp"
#include "image_functions.hpp"
#include "image_functions2.hpp"
#include "visu.hpp"

#include "brute2.hpp"
#include "pieces.hpp"
#include "compose2.hpp"

#include "timer.hpp"


extern int print_times;

struct mybitset
{
  vector<ull> data;
  mybitset(int n)
  {
    data.resize((n + 63) / 64);
  }
  int operator[](int i)
  {
    return data[i >> 6] >> (i & 63) & 1;
  }
  void set(int i, ull v)
  {
    int bit = i & 63;
    data[i >> 6] &= ~(1ull << bit);
    data[i >> 6] |= (v << bit);
  }
  ull hash()
  {
    ull r = 1;
    for (ull h : data)
    {
      r = r * 137139 + h;
    }
    return r;
  }
};

int popcount64c(ull x)
{
  const uint64_t m1 = 0x5555555555555555;  // binary: 0101...
  const uint64_t m2 = 0x3333333333333333;  // binary: 00110011..
  const uint64_t m4 = 0x0f0f0f0f0f0f0f0f;  // binary:  4 zeros,  4 ones ...
  const uint64_t m8 = 0x00ff00ff00ff00ff;  // binary:  8 zeros,  8 ones ...
  const uint64_t m16 = 0x0000ffff0000ffff; // binary: 16 zeros, 16 ones ...
  const uint64_t m32 = 0x00000000ffffffff; // binary: 32 zeros, 32 ones
  const uint64_t h01 = 0x0101010101010101; // the sum of 256 to the power of 0,1,2,3...
  x -= (x >> 1) & m1;                      // put count of each 2 bits into those 2 bits
  x = (x & m2) + ((x >> 2) & m2);          // put count of each 4 bits into those 4 bits
  x = (x + (x >> 4)) & m4;                 // put count of each 8 bits into those 8 bits
  return (x * h01) >> 56;                  // returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ...
}

int popcount64d(ull x)
{
  int pop = 0;
  while (x)
  {
    x &= x - 1;
    pop++;
  }
  return pop;
}

// Function: greedyCompose2
// Description: Attempts to compose images from pieces to match target images, using a greedy approach.
//
// Parameters:
// - pieces: A reference to a Pieces object containing the pieces to be used for composition.
// - target: A reference to a vector of Image objects that are the target compositions.
// - out_sizes: A vector of point objects representing the desired output sizes for the compositions.
//
// Returns: A vector of Candidate objects, each representing a potential solution by composing pieces 
//          to match the target images.
//
vector<Candidate> greedyCompose2(Pieces &pieces, vector<Image> &target, vector<point> out_sizes)
{
  // Return an empty vector if there are no pieces to compose
  if (pieces.piece.empty())
    return {};

  // Start a timer to measure the duration of the greedy fill operation
  Timer greedy_fill_time;

  // Ensure that pieces are sorted by depth in non-decreasing order
  {
    int d = -1;
    for (Piece3 &p : pieces.piece)
    {
      assert(p.depth >= d); // Assert that the current piece's depth is not less than the previous
      d = p.depth; // Update the depth tracker to the current piece's depth
    }
  }

  // Initialize vectors to hold the initial state of compositions and their sizes
  vector<Image> init;
  vector<int> sz;
  {
    // Loop through the directed acyclic graph (DAG) representing piece dependencies
    for (int i = 0; i < pieces.dag.size(); i++)
    {
      // Ensure that the output size matches the target size for available targets
      if (i < target.size())
        assert(out_sizes[i] == target[i].sz);

      // Initialize each composition with a default value (10) and the specified output size
      init.push_back(core::full(out_sizes[i], 10));
      sz.push_back(init.back().mask.size());
    }
  }

  vector<Candidate> rets;

  int n = pieces.piece.size();

  int M = 0;
  for (int s : sz)
    M += s;

  const int M64 = (M + 63) / 64;
  vector<ull> bad_mem, active_mem;
  vector<int> img_ind, bad_ind, active_ind;

  {
    // Reserve memory for active and bad bitsets to optimize performance
    active_mem.reserve(n * M64);
    bad_mem.reserve(n * M64);
    
    // Initialize a tiny hash map to keep track of seen configurations
    TinyHashMap seen;
    
    // Initialize bitsets for tracking bad and black (inactive) pixels
    mybitset badi(M), blacki(M);
    
    // Iterate over all pieces
    for (int i = 0; i < n; i++)
    {
      // Initialize counters for the current piece's bad and black pixels
      int x = 0, y = 0;
    
      // Iterate over each size category of pieces
      for (int j = 0; j < sz.size(); j++)
      {
        // Get the index to the current piece's memory location
        int *ind = &pieces.mem[pieces.piece[i].memi];
    
        // Retrieve the image corresponding to the current piece and size category
        Image_ img = pieces.dag[j].getImg(ind[j]);
    
        // Get the mask of the current piece's image
        const vector<char> &p = img.mask;
    
        // Determine the target mask, falling back to the initial mask if necessary
        const vector<char> &t = j < target.size() ? target[j].mask : init[j].mask;
    
        // Ensure the masks are of the expected size
        assert(p.size() == sz[j]);
        assert(t.size() == sz[j]);
    
        // Iterate over each pixel in the mask
        for (int k = 0; k < sz[j]; k++)
        {
          // Set bits in `badi` for pixels that differ between the piece and target masks
          badi.set(x++, (p[k] != t[k]));
    
          // Set bits in `blacki` for pixels that are black (inactive) in the piece's mask
          blacki.set(y++, (p[k] == 0));
        }
      }
    
      // Record the index of the current piece
      img_ind.push_back(i);
    
      // Record the starting index in `active_mem` for the current piece's black pixels
      active_ind.push_back(active_mem.size());
      // Copy the black pixel data to `active_mem`
      for (ull v : blacki.data)
        active_mem.push_back(v);
    
      // Record the starting index in `bad_mem` for the current piece's bad pixels
      bad_ind.push_back(bad_mem.size());
      // Copy the bad pixel data to `bad_mem`
      for (ull v : badi.data)
        bad_mem.push_back(v);
    }
  }

  int max_piece_depth = 0;
  for (int i = 0; i < n; i++)
    max_piece_depth = max(max_piece_depth, pieces.piece[i].depth);


  // Define a lambda function for the core greedy composition algorithm
  auto greedyComposeCore = [&](mybitset &cur, const mybitset &careMask, const int piece_depth_thres, vImage &ret)
  {
    // Initialize a vector to keep track of sparse indices in the bitmask
    vector<int> sparsej;
    // Iterate over each 64-bit segment of the bitmask
    for (int j = 0; j < M64; j++)
    {
      // If there's a bit that is unset in `cur` but set in `careMask`, add its index to `sparsej`
      if (~cur.data[j] & careMask.data[j])
        sparsej.push_back(j);
    }
  
    // Initialize vectors to keep track of the best and temporary active bit segments
    vector<ull> best_active(M64), tmp_active(M64);
    // Initialize the best index and the best count pair (used for scoring)
    int besti = -1;
    pair<int, int> bestcnt = {0, 0};
  
    // Iterate over all images
    for (int i = 0; i < img_ind.size(); i++)
    {
      // Skip images whose piece depth exceeds the threshold
      if (pieces.piece[img_ind[i]].depth > piece_depth_thres)
        continue;
  
      // Iterate over three states for each piece: normal, inverted, and full
      for (int k : {0, 1, 2})
      {
        // Pointer to the active data for the current image
        const ull *active_data = &active_mem[active_ind[i]];
  
        // Determine the flip and full masks based on the current state `k`
        ull flip = (k == 0 ? ~0 : 0); // Invert bits if k == 0
        ull full = (k == 2 ? ~0 : 0); // Set all bits if k == 2
  
        // Pointer to the bad data for the current image
        const ull *bad_data = &bad_mem[bad_ind[i]];
        // Initialize counters for the number of bits set and covered, and a flag for validity
        int cnt = 0, covered = 0, ok = 1;
        // Iterate over each 64-bit segment of the bitmask
        for (int j = 0; j < M64; j++)
        {
          // Calculate the active bits, applying flip and full masks
          ull active = ((active_data[j] ^ flip) | full);
          // Check if there are any bad bits that are also active and not yet covered in `cur`
          if (~cur.data[j] & bad_data[j] & active)
          {
            // If such bits are found, the current configuration is not valid
            ok = 0;
            break; // Exit the loop early as we found a conflict
          }
        }
        // Further processing would go here, such as updating the best counters and active segments
        if (!ok)
          continue;
  
        // Iterate over each index in the sparsej vector
        for (int j : sparsej)
        {
          // Calculate the active bits by XORing with flip and ORing with full
          ull active = ((active_data[j] ^ flip) | full);
          // Count the number of bits that are active and not in the current data but are in the care mask
          cnt += popcount64d(active & ~cur.data[j] & careMask.data[j]);
        }

        // Check if the current configuration is valid and better than the best found so far
        if (ok && make_pair(cnt, -covered) > bestcnt)
        {
          // Update the best count and index
          bestcnt = make_pair(cnt, -covered);
          besti = i;

          // Update the temporary active data based on the value of k
          if (k == 0)
          {
            // Invert all bits in active_data
            for (int j = 0; j < M64; j++)
              tmp_active[j] = ~active_data[j];
          }
          else if (k == 1)
          {
            // Copy active_data as is
            for (int j = 0; j < M64; j++)
              tmp_active[j] = active_data[j];
          }
          else
          {
            // Set all bits to 1
            for (int j = 0; j < M64; j++)
              tmp_active[j] = ~0;
          }
          // Store the best active configuration
          best_active = tmp_active;
        }
      }
    }
  
    // If no valid index was found, return -1
    if (besti == -1)
      return -1;
  
    {
      // Retrieve the index of the best image and initialize a counter
      int i = img_ind[besti], x = 0;
      
      // Retrieve the depth of the best image
      int depth = pieces.piece[i].depth;
      
      // Iterate over each layer of the result image
      for (int l = 0; l < ret.size(); l++)
      {
        // Get the memory index for the current piece
        int *ind = &pieces.mem[pieces.piece[i].memi];
        // Retrieve the mask of the current piece for the current layer
        const vector<char> &mask = pieces.dag[l].getImg(ind[l]).mask;
        // Iterate over each pixel in the current layer
        for (int j = 0; j < sz[l]; j++)
        {
          // Check if the current pixel is active in the best configuration and is marked for update in the result mask
          if ((best_active[x >> 6] >> (x & 63) & 1) && ret[l].mask[j] == 10)
          {            
            // Update the pixel in the result mask with the pixel from the current piece's mask
            ret[l].mask[j] = mask[j];
          }
      
          // Increment the pixel counter
          x++;
        }
      }
      // Iterate over all pixels in the result image
      for (int j = 0; j < M; j++)
      {
        // Check if the current pixel is active in the best configuration
        if (best_active[j >> 6] >> (j & 63) & 1)
          // Set the current pixel as active in the current configuration
          cur.set(j, 1);
      }
      
      // Return the depth of the best image
      return depth;
    }
  };

  // Map to store images filled with black pixels based on their hash
  map<ull, Image> greedy_fill_mem;

  int maxiters = 10; // Maximum number of iterations for the greedy algorithm

  // Iterate over piece depths in steps of 10
  for (int pdt = max_piece_depth % 10; pdt <= max_piece_depth; pdt += 10)
  {
    int piece_depth_thres = pdt; // Set the current piece depth threshold

    // Perform 10 iterations for each piece depth
    for (int it0 = 0; it0 < 10; it0++)
    {
      // Iterate over all possible masks (up to 5 bits or the size of the target)
      for (int mask = 1; mask < min(1 << target.size(), 1 << 5); mask++)
      {
        vector<int> maskv;
        // Create a vector of indices where the mask has bits set
        for (int j = 0; j < target.size(); j++)
          if (mask >> j & 1)
            maskv.push_back(j);

        int caremask;
        // Determine the caremask based on the current iteration
        if (it0 < maskv.size())
        {
          caremask = 1 << maskv[it0];
        }
        else
        {
          continue; // Skip if the iteration exceeds the mask vector size
        }

        mybitset cur(M), careMask(M);
        {
          int base = 0;
          // Set bits in cur and careMask based on the mask and caremask
          for (int j = 0; j < sz.size(); j++)
          {
            if (!(mask >> j & 1))
              for (int k = 0; k < sz[j]; k++)
                cur.set(base + k, 1);
            if ((caremask >> j & 1))
              for (int k = 0; k < sz[j]; k++)
                careMask.set(base + k, 1);
            base += sz[j];
          }
        }

        int cnt_pieces = 0; // Counter for the number of pieces used
        vector<int> piece_depths; // Vector to store the depths of pieces used
        int sum_depth = 0, max_depth = 0; // Sum and maximum depth of pieces

        vector<Image> ret = init; // Initialize the result with the initial images
        for (int it = 0; it < maxiters; it++)
        {
          // Perform the core greedy composition algorithm
          int depth = greedyComposeCore(cur, careMask, piece_depth_thres, ret);
          if (depth == -1)
            break; // Exit if no valid depth is found

          piece_depths.push_back(depth);
          cnt_pieces++;
          sum_depth += depth;
          max_depth = max(max_depth, depth);

          {
            greedy_fill_time.start(); // Start timing the greedy fill process
            vImage cp = ret;
            int carei = 31 - __builtin_clz(caremask); // Find the index of the care bit
            assert(caremask == 1 << carei);
            int ok = 1;
            {
              Image &img = cp[carei];

              // Replace null characters in the mask with black
              for (char &c : img.mask)
                if (c == 10)
                  c = 0;

              img = greedyFillBlack(img); // Fill the image with black pixels
              if (img != target[carei])
                ok = 0; // Mark as not okay if the filled image does not match the target
            }

            if (ok)
            {
              // Process the remaining images
              for (int i = 0; i < cp.size(); i++)
              {
                if (i == carei)
                  continue;
                Image &img = cp[i];

                // Replace null characters in the mask with black
                for (char &c : img.mask)
                  if (c == 10)
                    c = 0;

                ull h = hashImage(img); // Compute the hash of the image
                if (!greedy_fill_mem.count(h))
                {
                  greedy_fill_mem[h] = greedyFillBlack(img); // Fill the image with black pixels if not already done
                }
                
                img = greedy_fill_mem[h];
                if (img.w * img.h <= 0)
                  ok = 0; // Mark as not okay if the image dimensions are invalid
              }
              if (ok)
                rets.emplace_back(cp, cnt_pieces + 1, sum_depth, max_depth); // Add the result to the list of results
            }
            greedy_fill_time.stop(); // Stop timing the greedy fill process
          }
        }

        // Add the final result to the list of results
        rets.emplace_back(ret, cnt_pieces, sum_depth, max_depth);
      }
    }
  }

  if (print_times)
    greedy_fill_time.print("Greedy fill time");

  return rets;
}

// Function: composePieces2
// Description: Generates candidate solutions by composing pieces to match target images from training data.
//
// Parameters:
// - pieces: A reference to a Pieces object containing the pieces to be used for composition.
// - train: A vector of pairs, each containing an input Image and a corresponding output Image from the training data.
// - out_sizes: A vector of point objects representing the desired output sizes for the compositions.
//
// Returns: A vector of Candidate objects, each representing a potential solution by composing pieces to match the target images.
vector<Candidate> composePieces2(Pieces &pieces, vector<pair<Image, Image>> train, vector<point> out_sizes)
{  
  vector<Image> target; // Create a vector to hold the target images from the training data
  for (auto [in, out] : train) // Loop through the training data
  {
    target.push_back(out); // Add the output image (target) to the target vector
  }

  vector<Candidate> cands; // Initialize an empty vector to store the resulting candidates
  for (const Candidate &cand : greedyCompose2(pieces, target, out_sizes)) // Generate candidates using the greedyCompose2 function
  {
    cands.push_back(cand); // Add each generated candidate to the candidates vector
  }

  // Return the vector of generated candidates
  return cands; 
}

// Function: evaluateCands
// Description: Evaluates each candidate in the given vector of candidates based on a scoring system.
// Parameters:
// - cands: A vector of Candidate objects to be evaluated.
// - train: A vector of pairs, each containing an Image object representing the training data.
// Returns: A vector of Candidate objects that have been scored and potentially filtered based on the evaluation criteria.
vector<Candidate> evaluateCands(const vector<Candidate> &cands, vector<pair<Image, Image>> train)
{
  vector<Candidate> ret; // Vector to store the evaluated candidates
  for (const Candidate &cand : cands) // Iterate through each candidate
  {
    vImage_ imgs = cand.imgs; // Extract images from the candidate
    assert(cand.max_depth >= 0 && cand.max_depth < 100); // Ensure max_depth is within expected range
    assert(cand.cnt_pieces >= 0 && cand.cnt_pieces < 100); // Ensure cnt_pieces is within expected range
    
    // Calculate a prior based on the candidate's max depth and count of pieces
    double prior = cand.max_depth + cand.cnt_pieces * 1e-3; 

    int goods = 0; // Counter for "good" matches
    for (int i = 0; i < train.size(); i++) // Iterate through the training data
    {
      goods += (imgs[i] == train[i].second); // Increment goods if the candidate's image matches the training data
    }
    double score = goods - prior * 0.01; // Calculate the candidate's score

    Image answer = imgs.back(); // Get the last image from the candidate's images
    
    // Invalidate the candidate if the image dimensions are too large or if the area is zero
    if (answer.w > 30 || answer.h > 30 || answer.w * answer.h == 0)
      goods = 0;

    for (int i = 0; i < answer.h; i++) // Iterate through the height of the image
    {
      for (int j = 0; j < answer.w; j++) // Iterate through the width of the image
      {
        if (answer(i, j) < 0 || answer(i, j) >= 10) // Invalidate the candidate if any pixel value is out of range
          goods = 0;
      }
    }

    if (goods) // Add the candidate to the result vector if it is considered "good"
      ret.emplace_back(imgs, score);
  }

  sort(ret.begin(), ret.end()); // Sort the result vector based on the candidate scores
  // printf("%.20f\n\n", ret[0].score);
  return ret; // Return the vector of evaluated candidates
}
