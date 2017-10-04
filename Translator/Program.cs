using System;
using System.Diagnostics;
using System.IO.Pipes;
using EasyHook;
using System.IO;

namespace Translator
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Welcome to DotA 2 Translator");

            int targetPID = 0;
            Process[] processes = Process.GetProcesses();
            for (int i = 0; i < processes.Length; i++)
            {
                try
                {
                    if (processes[i].MainWindowTitle == "Dota 2" && processes[i].HasExited == false)
                    {
                        targetPID = processes[i].Id;
                    }
                }
                catch { }
            }
            if (targetPID == 0)
            {
                Console.WriteLine("You see, you need to run the game for the translator to do something");
                Console.ReadKey();
                Environment.Exit(-1);
            }

            try
            {
                NativeAPI.RhInjectLibrary(targetPID, 0, NativeAPI.EASYHOOK_INJECT_DEFAULT, "", ".\\Injectee.dll", IntPtr.Zero, 0);
            }
            catch (Exception e)
            {
                Console.WriteLine(e.Message);
                Console.ReadKey();
                Environment.Exit(-1);
            }

            Console.WriteLine("GLHF!");

            while (true)
            {
                var server = new NamedPipeServerStream("DotATranslator");
                server.WaitForConnection();
                StreamReader reader = new StreamReader(server);
                while (true)
                {
                    var chatMsg = reader.ReadLine();
                    Console.WriteLine(chatMsg);
                }
            }
        }
    }
}
