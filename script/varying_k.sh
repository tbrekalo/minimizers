# w = 8
for k in {5..100}
do
	./density -i random.10M.fa -k $k -w 8 -a mod-minimizer --stream 2>> mod_mini.varying_k.w.8.txt
done

# w = 8
for k in {5..100}
do
	./density -i random.10M.fa -k $k -w 8 -a lr-minimizer --stream 2>> lr_mini.varying_k.w.8.txt
done

# w = 8
for k in {5..100}
do
	./density -i random.10M.fa -k $k -w 8 -a minimizer --stream 2>> mini.varying_k.w.8.txt
done

# w = 8
for k in {5..100}
do
	./density -i random.10M.fa -k $k -w 8 -a miniception --stream 2>> miniception.varying_k.w.8.txt
done

# w = 8
for k in {5..100}
do
    ./density -i random.10M.fa -k $k -w 8 -a rot-minimizer --stream 2>> rot-minimizer.varying_k.w.8.txt
done

# w = 24
for k in {5..300}
do
	./density -i random.10M.fa -k $k -w 24 -a mod-minimizer --stream 2>> mod_mini.varying_k.w.24.txt
done

# w = 24
for k in {5..300}
do
	./density -i random.10M.fa -k $k -w 24 -a lr-minimizer --stream 2>> lr_mini.varying_k.w.24.txt
done

# w = 24
for k in {5..300}
do
	./density -i random.10M.fa -k $k -w 24 -a minimizer --stream 2>> mini.varying_k.w.24.txt
done

# w = 24
for k in {5..300}
do
	./density -i random.10M.fa -k $k -w 24 -a miniception --stream 2>> miniception.varying_k.w.24.txt
done

# w = 24
for k in {5..300}
do
    ./density -i random.10M.fa -k $k -w 24 -a rot-minimizer --stream 2>> rot-minimizer.varying_k.w.24.txt
done