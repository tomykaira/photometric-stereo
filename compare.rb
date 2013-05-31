hint = File.readlines("dist/hint.txt")
result = File.readlines("result.txt")

(0...80).each do |line|
  name = 'output/%03d.csv' % line
  File.open(name, "w") do |f|
    300.times do |c|
      f.puts("#{c}, #{hint[300*line+c].strip}, #{result[300*line+c].strip}")
    end
  end
  command = ["set terminal unknown", "plot \"#{name}\" using 1:2", "replot \"#{name}\" using 1:3", "set terminal png", ('set out "output/%03d.png"' % line), "replot"]
  system("echo '#{command.join("\n")}' | gnuplot")
end
