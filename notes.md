## To run a problem using the C++ program:

<code>
make run<br>
./run dataset/training/0a938d79.json 3<br>
</code>

This will generate a "output/answer_-1_3.csv" file


## To run a problem using the Python program (which tries 4 different options):

<code>
cd notebooks/absres-c-files<br>
python safe_run.py dataset/training/0a938d79.json 1<br>
</code>

This generates sample output in: notebooks/absres-c-files/store/tmp/0a938d79.out (and .err)<br>
and produces its predictions in: output/answer_-1_3.csv (etc. - one file per depth type)<br>
and creates its submission file in: notebooks/absres-c-files/submission_part.csv<br>

In the Jupyter Notebook this "submission_part.csv" is copied to a file "old_submission.csv" and from there translated into the new submission format,
creating a file: "sub_icecube.json"


# Results:

7/8/24 - 182/400 Training Problems Correct
         122/400 Evaluation Problems Correct
         24/100  Test Problems Correct
