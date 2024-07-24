#include "precompiled_stl.hpp"

using namespace std;

#include "utils.hpp"
#include "core_functions.hpp"
#include "image_functions.hpp"
#include "visu.hpp"
#include "read.hpp"
#include "normalize.hpp"
#include "tasks.hpp"
#include "evals.hpp"

#include "brute2.hpp"

#include "score.hpp"
#include "load.hpp"

#include "deduce_op.hpp"
#include "pieces.hpp"
#include "compose2.hpp"

#include "brute_size.hpp"

#include <thread>

string green(string s)
{
  return ("\033[1;32m" + s + "\033[0m");
}
string blue(string s)
{
  return ("\033[1;34m" + s + "\033[0m");
}
string yellow(string s)
{
  return ("\033[1;33m" + s + "\033[0m");
}
string red(string s)
{
  return ("\033[1;31m" + s + "\033[0m");
}

void writeVerdict(int si, string sid, int verdict)
{
  printf("Task #%2d (%s): ", si, sid.c_str());
  switch (verdict)
  {
  case 3:
    cout << green("Correct") << endl;
    break;
  case 2:
    cout << yellow("Candidate") << endl;
    break;
  case 1:
    cout << blue("Dimensions") << endl;
    break;
  case 0:
    cout << red("Nothing") << endl;
    break;
  default:
    assert(0);
  }
}

int MAXDEPTH = -1; // Argument

int MAXSIDE = 100, MAXAREA = 40 * 40, MAXPIXELS = 40 * 40 * 5; // Just default values

int print_times = 1, print_mem = 1, print_nodes = 1;

// Function to run various search and evaluation strategies on a set of samples
void runFnSearch(vector<Sample> sample, int only_sid = -1, int arg = -1, int eval = 0)
{
  // The following commented-out lines represent various operations or function calls that might be part of the search or evaluation process.
  // These operations are currently not executed, but they hint at the possible functionalities that can be included in this function.
  // rankFeatures();
  // evalNormalizeRigid();
  // evalTasks();
  // bruteSubmission();
  // bruteSolve();
  // evalEvals(1);
  // deduceEvals();

  // Determine if normalization should be skipped based on the argument value
  int no_norm = (arg >= 10 && arg < 20);
  // Determine if flips should be added to the search strategy based on the argument value
  int add_flips = (arg >= 20 && arg < 40);
  // Determine the flip ID to use based on the argument value
  int add_flip_id = (arg >= 30 && arg < 40 ? 7 : 6);

  // If no specific argument is provided, default the argument to 2
  if (arg == -1)
    arg = 2;
  // Set the maximum depth for the search based on the argument value
  MAXDEPTH = arg % 10 * 10;

  // Initialize a counter for skipped operations or samples
  int skips = 0;
  int scores[4] = {};

  Visu visu;

  vector<int> verdict(sample.size());

  int dones = 0;
  Loader load(sample.size());

  // Remember to fix Timers before running parallel

  for (int si = 0; si < sample.size(); si++)
  {
    if (only_sid != -1 && si != only_sid)
      continue;

    if (eval)
      load();
    else if (++dones % 10 == 0)
    {
      cout << dones << " / " << sample.size() << endl;
    }

    const Sample &s = sample[si];

    cout << "Processing: " << s.id << endl;

    // Normalize sample
    Simplifier sim = normalizeCols(s.train);
    if (no_norm)
      sim = normalizeDummy(s.train);

    vector<pair<Image, Image>> train;
    for (auto &[in, out] : s.train)
    {
      train.push_back(sim(in, out));
    }

    if (add_flips)
    {
      for (auto &[in, out] : s.train)
      {
        auto [rin, rout] = sim(in, out);
        train.push_back({rigid(rin, add_flip_id), rigid(rout, add_flip_id)});
      }
    }

    auto [test_in, test_out] = sim(s.test_in, s.test_out);

    // the index of the image to write for testing
    int testIndex = 1;
    if (testIndex >= 0)
    {
      write(s.train[testIndex].first, "s_train_first.csv");
      write(s.train[testIndex].second, "s_train_second.csv");

      cout << "Original Input" << endl;
      print(s.train[testIndex].first);

      cout << "Normalised Input" << endl;
      print(train[testIndex].first);

      cout << "Normalised Output" << endl;
      print(train[testIndex].second);

      write(train[testIndex].first, "train_first_norm.csv");
      write(train[testIndex].second, "train_second_norm.csv");

      write(s.test_in, "test_in.csv");
      write(s.test_out, "test_out.csv");
      write(test_in, "test_in_norm.csv");
      write(test_out, "test_out_norm.csv");
    }

    {
      // Initialize variables for sum of input sizes, sum of output sizes, maximum number of colors,
      // maximum side length, and maximum area
      int insumsz = 0, outsumsz = 0, macols = 0;
      int maxside = 0, maxarea = 0;

      // Iterate over each training example
      for (auto &[in, out] : s.train)
      {
        // Update maximum side length and area based on current example
        maxside = max({maxside, in.w, in.h, out.w, out.h});
        maxarea = max({maxarea, in.w * in.h, out.w * out.h});

        // Accumulate total input and output sizes
        insumsz += in.w * in.h;
        outsumsz += out.w * out.h;

        // Update maximum number of colors
        macols = max(macols, __builtin_popcount(core::colMask(in)));
      }
      // Determine the larger sum size between input and output
      int sumsz = max(insumsz, outsumsz);

      // Log features to standard error
      cerr << "Features: " << insumsz << ' ' << outsumsz << ' ' << macols << endl;

      // Initialize weights for estimating execution time
      double w[4] = {1.2772523019346949, 0.00655104, 0.70820414, 0.00194519};

      // Estimate execution time based on sum size and maximum number of colors
      double expect_time3 = w[0] + w[1] * sumsz + w[2] * macols + w[1] * w[2] * sumsz * macols;

      // Log the maximum depth to standard error
      cerr << "MAXDEPTH: " << MAXDEPTH << endl;

      // Set constants for maximum side, area, and pixels
      MAXSIDE = 100;
      MAXAREA = maxarea * 2;
      MAXPIXELS = MAXAREA * 5;
    }

    // Generate output sizes for brute force size estimation
    vector<point> out_sizes = bruteSize(test_in, train);

    // Generate candidate pieces
    Pieces pieces;
    {
      // Record start time for performance measurement
      double start_time = now();

      // Generate directed acyclic graphs (DAGs) for brute force piece generation
      vector<DAG> dags = brutePieces2(test_in, train, out_sizes);

      // If time logging is enabled, print the time taken to generate pieces
      if (print_times)
        cout << "brutePieces time: " << now() - start_time << endl;

      // Record start time for making pieces
      start_time = now();

      // Create pieces from DAGs
      pieces = makePieces2(dags, train, out_sizes);

      // If time logging is enabled, print the time taken to make pieces
      if (print_times)
        cout << "makePieces time: " << now() - start_time << endl;
    }

    // Check if memory logging is enabled
    if (print_mem)
    {
      // Initialize variables for tracking memory usage in different components
      double size = 0, child = 0, other = 0, inds = 0, maps = 0;

      // Iterate over each DAG in the pieces collection
      for (DAG &d : pieces.dag)
      {
        // Accumulate memory used by TinyNode objects
        other += sizeof(TinyNode) * d.tiny_node.size();

        // Accumulate memory used by the memory bank in TinyNode
        size += 4 * d.tiny_node.bank.mem.size();

        // Iterate over each TinyNode to calculate memory used by children
        for (TinyNode &n : d.tiny_node.node)
        {
          // Use different memory calculation based on the size of children
          if (n.child.sz < TinyChildren::dense_thres)
            child += n.child.cap * 8; // For sparse children
          else
            child += n.child.cap * 4; // For dense children
        }

        // Accumulate memory used by hash maps in DAGs
        maps += 16 * d.hashi.data.size() + 4 * d.hashi.table.size();
      }

      // Calculate memory used by Piece3 objects
      for (Piece3 &p : pieces.piece)
      {
        inds += sizeof(p);
      }

      // Add memory used by the pieces' memory vector
      inds += sizeof(pieces.mem[0]) * pieces.mem.size();

      // Print total memory usage for each component in megabytes
      printf("Memory: %.1f + %.1f + %.1f + %.1f + %.1f MB\n", size / 1e6, child / 1e6, other / 1e6, maps / 1e6, inds / 1e6);
    }

    // Clear hash maps and children in all DAGs to free memory
    for (DAG &d : pieces.dag)
    {
      d.hashi.clear();
      for (TinyNode &n : d.tiny_node.node)
      {
        n.child.clear();
      }
    }

    // Initialize a flag to check if the last output size matches the test output size
    int s1 = 0;

    // If not in evaluation mode, set s1 based on the match of output sizes
    if (!eval)
      s1 = (out_sizes.back() == test_out.sz);

    // Begin assembling pieces into candidates
    vector<Candidate> cands;
    {
      // Record start time for performance measurement
      double start_time = now();

      // Compose pieces into candidates based on the training data and output sizes
      cands = composePieces2(pieces, train, out_sizes);

      // If time logging is enabled, print the time taken to compose pieces
      if (print_times)
        cout << "composePieces time: " << now() - start_time << endl;
    }

    // Deduce outer products from pieces and candidates
    addDeduceOuterProduct(pieces, train, cands);

    // Evaluate candidates based on training data
    cands = evaluateCands(cands, train);

    // Initialize score for candidates
    int s2 = 0;

    // If not in evaluation mode, calculate score for candidates based on test input and output
    if (!eval)
      s2 = scoreCands(cands, test_in, test_out);

    // Initially set answers to all candidates
    vector<Candidate> answers = cands;

    {
      // Sort candidates based on their score or other criteria
      sort(cands.begin(), cands.end());

      // Filtered candidates to remove duplicates
      vector<Candidate> filtered;

      // Set to track seen images to avoid duplicates
      set<ull> seen;

      // Iterate over sorted candidates
      for (const Candidate &cand : cands)
      {
        // Hash the last image of the candidate for uniqueness check
        ull h = hashImage(cand.imgs.back());

        // If hash is unique, add candidate to filtered list
        if (seen.insert(h).second)
        {
          filtered.push_back(cand);

          // Limit the number of filtered candidates
          if (filtered.size() == 3 + skips * 3)
            break;
        }
      }

      // Remove extra candidates based on the number of skips
      for (int i = 0; i < skips * 3 && filtered.size(); i++)
        filtered.erase(filtered.begin());

      // Finalize answers by moving filtered candidates
      answers = move(filtered);
    }

    // Reconstruct answers from candidates
    vector<Image> rec_answers;

    // Scores for the reconstructed answers
    vector<double> answer_scores;

    // Iterate over final answers to reconstruct images and calculate scores
    for (Candidate &cand : answers)
    {
      // Reconstruct image from candidate
      rec_answers.push_back(sim.rec(s.test_in, cand.imgs.back()));

      // Get score of the candidate
      double score = cand.score;

      // Adjust score if flips are added to the evaluation
      if (add_flips)
      {
        score /= 2 - 1e-5;
      }

      // Store adjusted score
      answer_scores.push_back(score);
    }

    int s3 = 0;
    // Calculate the score for the reconstructed answers if not in evaluation mode
    if (!eval)
    {
      // Score the reconstructed answers against the test input and output
      s3 = scoreAnswers(rec_answers, s.test_in, s.test_out);
    }

    // Write the test output to a CSV file for further analysis or review
    write(s.test_out, "test_out_3.csv");

    // Visualization and scoring logic, executed only if not in evaluation mode
    if (!eval)
    {
      // Begin a new visualization section for the test case
      visu.next(to_string(si) + " - test");

      // Add each training example to the visualization
      for (auto &[in, out] : train)
        visu.add(in, out);

      // Begin a new section for visualizing the test input and output
      visu.next(to_string(si) + " - test");
      visu.add(test_in, test_out);

      // Begin a new section for visualizing the candidate solutions
      visu.next(to_string(si) + " - cands");

      // Add up to 5 candidate solutions to the visualization
      for (int i = 0; i < min((int)answers.size(), 5); i++)
      {
        visu.add(test_in, answers[i].imgs.back());
      }
    }

    // Update the verdict based on the scores if not in evaluation mode
    if (!eval)
    {
      // Set the verdict based on the highest score achieved
      if (s3)
        verdict[si] = 3; // Highest confidence in the solution
      else if (s2)
        verdict[si] = 2; // Medium confidence
      else if (s1)
        verdict[si] = 1; // Low confidence
      else
        verdict[si] = 0; // No confidence, possibly incorrect solution

      // Increment the count for the achieved score
      scores[verdict[si]]++;

      // Write the verdict for the current sample to a file for review
      writeVerdict(si, s.id, verdict[si]);
    }

    // Write the answers along with their scores to a CSV file for further analysis
    {
      // Construct the filename based on the sample ID and an argument
      string fn = "output/answer_" + to_string(only_sid) + "_" + to_string(arg) + ".csv";
      // Write the answers and their scores to the file
      writeAnswersWithScores(s, fn, rec_answers, answer_scores);
    }
  }

  if (!eval && only_sid == -1)
  {
    for (int si = 0; si < sample.size(); si++)
    {
      Sample &s = sample[si];
      writeVerdict(si, s.id, verdict[si]);
    }
    for (int si = 0; si < sample.size(); si++)
      cout << verdict[si];
    cout << endl;

    for (int i = 3; i; i--)
      scores[i - 1] += scores[i];

    printf("\n");
    printf("Total: % 4d\n", scores[0]);
    printf("Pieces:% 4d\n", scores[1]);
    printf("Cands: % 4d\n", scores[2]);
    printf("Correct:% 3d\n", scores[3]);
  }
}

void runSingleFile(const string &aFileName, int arg = -1)
{
  vector<Sample> sample = readSingleFile(aFileName);
  if (sample.size() == 1)
  {
    runFnSearch(sample, -1, arg);
  }
}

void run(int only_sid = -1, int arg = -1)
{

  int eval = 0;

  // string sample_dir = "evaluation";
  string sample_dir = "training";
  int samples = -1;
  if (eval)
  {
    sample_dir = "test";
    samples = -1;
  }

  vector<Sample> sample = readAll(sample_dir, samples);
  assert(only_sid < sample.size());

  runFnSearch(sample, only_sid, arg, eval);
}
