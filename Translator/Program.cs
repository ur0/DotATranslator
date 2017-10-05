using System;
using System.Diagnostics;
using System.IO.Pipes;
using System.Net;
using EasyHook;
using System.IO;
using System.Net.Http;
using Newtonsoft.Json.Linq;
using IniParser;
using IniParser.Model;

namespace Translator
{
    class Program
    {
        static string TargetLang = "";

        static void Main(string[] args)
        {
            var parser = new FileIniDataParser();
            IniData data = parser.ReadFile("Config.ini");
            TargetLang = data["Translation"]["lang"];

            Console.WriteLine("Welcome to DotA 2 Translator");
            Console.WriteLine("Translating to: " + TargetLang);

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

            var server = new NamedPipeServerStream("DotATranslator");

            try
            {
                NativeAPI.RhInjectLibrary(targetPID, 0, NativeAPI.EASYHOOK_INJECT_DEFAULT, "", ".\\Injectee.dll", IntPtr.Zero, 0);
            }
            catch (Exception e)
            {
                Console.WriteLine(e.Message);
                Console.Read();
                Environment.Exit(-1);
            }

            ListenPipeAndTranslate(server);
            Console.Read();
        }

        static async void ListenPipeAndTranslate(NamedPipeServerStream server)
        {
            var sr = new StreamReader(server);
            do
            {
                try
                {
                    await server.WaitForConnectionAsync();
                    Console.WriteLine("GLHF!");
                    char[] buffer = new char[1000];
                    while (true)
                    {
                        System.Text.StringBuilder sb = new System.Text.StringBuilder();
                        await sr.ReadAsync(buffer, 0, 1000);
                        int endIdx = 0;
                        for (int i = 0; i < 1000; i++)
                        {
                            if (buffer[i] == '\0')
                            {
                                endIdx = i;
                                break;
                            }
                        }
                        sb.Append(buffer, 0, endIdx + 1);
                        Console.Write("Incoming: ");
                        Console.WriteLine(sb);
                        TranslateAndPrint(sb.ToString());
                    }
                }
                catch (Exception)
                {
                    Console.WriteLine("Injectee disconnected, exiting");
                    Environment.Exit(0);
                }
                finally
                {
                    server.WaitForPipeDrain();
                    if (server.IsConnected) { server.Disconnect(); }
                }
            } while (true);
        }

        static async void TranslateAndPrint(string message)
        {
            if (message.Contains("<img") || message.Contains("<font"))
            {
                return;
            }

            try
            {
                string url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=fr&dt=t&q=" + WebUtility.UrlEncode(message);
                HttpClient hc = new HttpClient();
                HttpResponseMessage r = await hc.GetAsync(url);
                String rs = await r.Content.ReadAsStringAsync();
                dynamic d = JArray.Parse(rs);
                string translated = d[0][0][0];
                string sourcelang = d[2];
                Console.Write("Translated from " + sourcelang + ": ");
                Console.WriteLine(translated);
            } catch (Exception)
            {
                Console.WriteLine("Unable to translate, check your connection");
                return;
            }
        }
    }
}
