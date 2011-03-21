require 'rmem'

class Memory

  def self.usage(name)
    GC.start
    GC.disable
    smem = RMem::Report.memory

    t = Time.now
    yield
    duration = Time.now - t

    GC.enable
    GC.start

    emem = RMem::Report.memory
    memory_usage = emem / 1024.0
    memory_growth = (emem - smem) / 1024.0

    printf "#{name}\t\tDuration: %.4f sec, Memory Usage: %.2f KB - Memory Growth: %.2f KB\n", duration, memory_usage, memory_growth
  end

end
