#region

using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading;
using EasyHook;
using IniParser;
using Newtonsoft.Json.Linq;

#endregion

namespace Translator
{
    internal class Program
    {
        private static string _targetLang = "";

        private static void Main()
        {
            try
            {
                var parser = new FileIniDataParser();
                var data = parser.ReadFile("Config.ini");
                _targetLang = data["Translation"]["lang"];
            }
            catch
            {
                Console.WriteLine("Can't find Config.ini, creating one.");
                File.AppendAllText("Config.ini", "[Translation]" + Environment.NewLine + "lang=en");
                Console.WriteLine("A new configuration file has been created.");
                Console.WriteLine("Please edit Config.ini if you wish to change the language (currently English).");
                _targetLang = "en";
            }

            Console.WriteLine("Welcome to DotA 2 Translator (v2)");
            Console.WriteLine("Translating to: " + _targetLang + " (edit Config.ini to change this).");

            var targetPid = 0;
            var processes = Process.GetProcesses();
            foreach (var t in processes)
            {
                try
                {
                    if (t.MainWindowTitle == "Dota 2" && t.HasExited == false)
                    {
                        targetPid = t.Id;
                    }
                }
                catch
                {
                }
            }
            if (targetPid == 0)
            {
                Console.WriteLine("You see, you need to run the game for the translator to do something!");
                Console.WriteLine("Please start the translator after you start DotA 2");
                Console.WriteLine("Press any key to close");
                Console.ReadKey();
                Environment.Exit(-1);
            }

            // Pipe 1 is used for game->translator communication
            // Pipe 2 is used for translator->game communication
            // I was having concurrency issues with using a single pipe
            // Also, these pipes need to exist before the injection occurs (please don't move this, it'll lead to races)
            var server1 = new NamedPipeServerStream("DotATranslator1");
            var server2 = new NamedPipeServerStream("DotATranslator2");

            try
            {
                NativeAPI.RhInjectLibrary(targetPid, 0, NativeAPI.EASYHOOK_INJECT_DEFAULT, "", ".\\Injectee.dll",
                    IntPtr.Zero, 0);
            }
            catch (Exception e)
            {
                Console.WriteLine(
                    "Could not inject our DLL into DotA 2. Please post the following message on /r/dotatranslator for help");
                Console.WriteLine(e.Message);
                Console.ReadKey();
                Environment.Exit(-1);
            }

            ListenPipeAndTranslate(server1, server2);
            Console.ReadKey();
            Environment.Exit(0);
        }

        private static async void ListenPipeAndTranslate(NamedPipeServerStream server1, NamedPipeServerStream server2)
        {
            do
            {
                try
                {
                    await server1.WaitForConnectionAsync();
                    await server2.WaitForConnectionAsync();
                    Console.WriteLine("GLHF!");
                    while (server1.IsConnected && server2.IsConnected)
                    {
                        var buffer = new byte[1000];
                        server1.Read(buffer, 0, 2);
                        var numBytes = BitConverter.ToUInt16(buffer, 0);
                        server1.Read(buffer, 0, numBytes);
                        Array.Resize(ref buffer, numBytes);

                        if (numBytes == 0)
                        {
                            continue;
                        }

                        var messageParams = new byte[100];
                        server1.Read(messageParams, 0, 2);
                        numBytes = BitConverter.ToUInt16(messageParams, 0);
                        server1.Read(messageParams, 0, numBytes);
                        Array.Resize(ref messageParams, numBytes);

                        TranslateAndPrint(Encoding.Unicode.GetString(buffer), server2, messageParams);
                    }
                }
                catch (Exception e)
                {
                    Console.WriteLine(e.ToString());
                    Console.WriteLine("DotA 2 has exited - press any key to close.");
                    return;
                }
            } while (true);
        }

        private static async void TranslateAndPrint(string message, Stream sw, byte[] messageParams)
        {
            // These messages are already in the user's language
            // They contain HTML too, so I'm not gonna bother translate them
            if (message.Contains("<img") || message.Contains("<font"))
            {
                return;
            }

            try
            {
                var url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=" + _targetLang +
                          "&dt=t&q=" + WebUtility.UrlEncode(message);
                var hc = new HttpClient();
                var r = await hc.GetAsync(url);
                var rs = await r.Content.ReadAsStringAsync();
                dynamic d = JArray.Parse(rs);
                var translated = d[0][0][0];
                var sourcelang = d[2];
                Console.WriteLine("Original: " + message);
                var toSend = "(Translated from " + sourcelang + "): " + translated;
                if (sourcelang != _targetLang)
                {
                    var ue = new UnicodeEncoding(false, false, false);
                    var sendBytes = ue.GetBytes(toSend);
                    var size = BitConverter.GetBytes((ushort) sendBytes.Length);
                    // Write the number of raw bytes in this message
                    sw.Write(size, 0, 2);
                    // Write the UTF-16 encoded message
                    sw.Write(sendBytes, 0, sendBytes.Length);
                    // Write the message parameters
                    sw.Write(messageParams, 0, messageParams.Length);
                    sw.Flush();
                    Console.WriteLine(toSend);
                }
            }
            catch (Exception e)
            {
                // If you close DotA 2 before closing the translator
                Console.Write("Unable to translate, check your connection: ");
                Console.WriteLine(e.Message);
                Thread.Sleep(2000);
                Environment.Exit(-1);
            }
        }
    }
}